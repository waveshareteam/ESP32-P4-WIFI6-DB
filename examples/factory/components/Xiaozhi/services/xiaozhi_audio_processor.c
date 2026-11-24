/*
 * Derived from 78/xiaozhi-esp32 (MIT).
 *
 * SPDX-License-Identifier: MIT
 */

#include "xiaozhi_audio_processor.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "esp_afe_sr_models.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_vadn_models.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "model_path.h"

static const char *tag = "xiaozhi_audio";
static const char *afe_input_format = "M";

#define EVENT_WAKE_WORD_ENABLED (1U << 0)
#define EVENT_VOICE_ENABLED     (1U << 1)
#define EVENT_SHUTDOWN          (1U << 2)
#define EVENT_TASK_EXITED       (1U << 3)
#define EVENT_ACTIVE            (EVENT_WAKE_WORD_ENABLED | EVENT_VOICE_ENABLED)

#define FETCH_TIMEOUT_MS        (250U)
#define PROCESS_TASK_STACK_SIZE (8U * 1024U)
#define PROCESS_TASK_PRIORITY   (3U)

struct xiaozhi_audio_processor {
    SemaphoreHandle_t lifecycle_mutex;
    SemaphoreHandle_t afe_mutex;
    SemaphoreHandle_t input_mutex;
    SemaphoreHandle_t output_mutex;
    SemaphoreHandle_t callback_mutex;

    EventGroupHandle_t event_group;
    TaskHandle_t processing_task;

    srmodel_list_t *models;
    const esp_afe_sr_iface_t *afe_iface;
    esp_afe_sr_data_t *afe_data;

    atomic_bool ready;
    atomic_bool shutting_down;
    _Atomic size_t feed_frames;
    atomic_bool vad_speaking;

    int16_t *input_buffer;
    size_t input_size;
    size_t input_capacity;
    int16_t *output_buffer;
    size_t output_size;
    size_t output_capacity;

    int16_t *wake_word_cache;
    size_t wake_word_cache_size;
    size_t wake_word_cache_write;
    size_t wake_word_cache_capacity;
    int16_t callback_frame[XIAOZHI_AUDIO_PCM_FRAME_SAMPLES];

    char *wakenet_model_name;
    char **wake_words;
    size_t wake_word_count;

    xiaozhi_audio_processor_callbacks_t callbacks;
};

static bool lock_mutex(SemaphoreHandle_t mutex)
{
    return mutex != NULL && xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE;
}

static void unlock_mutex(SemaphoreHandle_t mutex)
{
    if (mutex != NULL) {
        xSemaphoreGive(mutex);
    }
}

static char *copy_string_range(const char *source, size_t length)
{
    if (source == NULL) {
        return NULL;
    }
    char *copy = malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, source, length);
    copy[length] = '\0';
    return copy;
}

static bool reserve_samples(
    int16_t **buffer,
    size_t *capacity,
    size_t required
)
{
    if (buffer == NULL || capacity == NULL) {
        return false;
    }
    if (required <= *capacity) {
        return true;
    }
    if (required > SIZE_MAX / sizeof(int16_t)) {
        return false;
    }

    size_t new_capacity = *capacity == 0 ? 64 : *capacity;
    while (new_capacity < required) {
        if (new_capacity > SIZE_MAX / 2) {
            new_capacity = required;
            break;
        }
        new_capacity *= 2;
    }

    int16_t *new_buffer = realloc(
        *buffer,
        new_capacity * sizeof(int16_t)
    );
    if (new_buffer == NULL) {
        return false;
    }
    *buffer = new_buffer;
    *capacity = new_capacity;
    return true;
}

static bool append_wake_word(
    xiaozhi_audio_processor_t *processor,
    const char *word,
    size_t length
)
{
    if (processor == NULL || word == NULL || length == 0) {
        return false;
    }

    char *copy = copy_string_range(word, length);
    if (copy == NULL) {
        return false;
    }
    char **new_words = realloc(
        processor->wake_words,
        (processor->wake_word_count + 1) * sizeof(char *)
    );
    if (new_words == NULL) {
        free(copy);
        return false;
    }
    processor->wake_words = new_words;
    processor->wake_words[processor->wake_word_count++] = copy;
    return true;
}

static void free_wake_words(xiaozhi_audio_processor_t *processor)
{
    if (processor == NULL) {
        return;
    }
    for (size_t index = 0; index < processor->wake_word_count; ++index) {
        free(processor->wake_words[index]);
    }
    free(processor->wake_words);
    processor->wake_words = NULL;
    processor->wake_word_count = 0;
}

static void delete_mutexes(xiaozhi_audio_processor_t *processor)
{
    if (processor == NULL) {
        return;
    }
    if (processor->callback_mutex != NULL) {
        vSemaphoreDelete(processor->callback_mutex);
        processor->callback_mutex = NULL;
    }
    if (processor->output_mutex != NULL) {
        vSemaphoreDelete(processor->output_mutex);
        processor->output_mutex = NULL;
    }
    if (processor->input_mutex != NULL) {
        vSemaphoreDelete(processor->input_mutex);
        processor->input_mutex = NULL;
    }
    if (processor->afe_mutex != NULL) {
        vSemaphoreDelete(processor->afe_mutex);
        processor->afe_mutex = NULL;
    }
    if (processor->lifecycle_mutex != NULL) {
        vSemaphoreDelete(processor->lifecycle_mutex);
        processor->lifecycle_mutex = NULL;
    }
}

static esp_err_t create_afe(xiaozhi_audio_processor_t *processor)
{
    const esp_partition_t *model_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        "model"
    );
    if (model_partition == NULL) {
        ESP_LOGE(
            tag,
            "Speech model partition 'model' is missing; run a full flash so "
            "the partition table and srmodels.bin are written"
        );
        return ESP_ERR_NOT_FOUND;
    }

    processor->models = esp_srmodel_init("model");
    if (processor->models == NULL) {
        ESP_LOGE(
            tag,
            "Speech model partition exists but could not be loaded; run a "
            "full flash including srmodels.bin"
        );
        return ESP_FAIL;
    }
    if (processor->models->num <= 0) {
        ESP_LOGE(
            tag,
            "Speech model image is empty or unreadable "
            "(count=%d, offset=0x%08lx); rebuild and full-flash the "
            "partition table and srmodels.bin",
            processor->models->num,
            (unsigned long)model_partition->address
        );
        return ESP_ERR_INVALID_SIZE;
    }
    ESP_LOGI(
        tag,
        "Loaded %d speech model(s) from offset 0x%08lx",
        processor->models->num,
        (unsigned long)model_partition->address
    );

    char *wakenet_model = esp_srmodel_filter(
        processor->models,
        ESP_WN_PREFIX,
        NULL
    );
    if (wakenet_model == NULL) {
        ESP_LOGE(tag, "No WakeNet model found");
        return ESP_ERR_NOT_FOUND;
    }
    processor->wakenet_model_name = copy_string_range(
        wakenet_model,
        strlen(wakenet_model)
    );
    if (processor->wakenet_model_name == NULL) {
        return ESP_ERR_NO_MEM;
    }

    const char *words = esp_srmodel_get_wake_words(
        processor->models,
        wakenet_model
    );
    if (words != NULL) {
        const char *begin = words;
        while (*begin != '\0') {
            const char *end = strchr(begin, ';');
            if (end == NULL) {
                end = begin + strlen(begin);
            }
            if (end > begin && !append_wake_word(
                    processor,
                    begin,
                    (size_t)(end - begin)
                )) {
                return ESP_ERR_NO_MEM;
            }
            if (*end == '\0') {
                break;
            }
            begin = end + 1;
        }
    }

    char *vad_model = esp_srmodel_filter(
        processor->models,
        ESP_VADN_PREFIX,
        NULL
    );
    afe_config_t *config = afe_config_init(
        afe_input_format,
        processor->models,
        AFE_TYPE_FD,
        AFE_MODE_LOW_COST
    );
    if (config == NULL) {
        ESP_LOGE(tag, "Failed to create AFE configuration");
        return ESP_ERR_NO_MEM;
    }

    config->aec_init = false;
    config->ns_init = false;
    config->vad_init = true;
    config->vad_mode = VAD_MODE_0;
    config->vad_min_noise_ms = 100;
    if (vad_model != NULL) {
        config->vad_model_name = vad_model;
    }
    config->wakenet_init = true;
    config->wakenet_model_name = wakenet_model;
    config->agc_init = false;
    config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;

    processor->afe_iface = esp_afe_handle_from_config(config);
    if (processor->afe_iface != NULL) {
        processor->afe_data = processor->afe_iface->create_from_config(config);
    }
    afe_config_free(config);

    if (processor->afe_iface == NULL || processor->afe_data == NULL) {
        ESP_LOGE(tag, "Failed to create AFE instance");
        processor->afe_iface = NULL;
        processor->afe_data = NULL;
        return ESP_FAIL;
    }

    processor->afe_iface->disable_wakenet(processor->afe_data);
    size_t feed_frames = (size_t)processor->afe_iface->get_feed_chunksize(
        processor->afe_data
    );
    if (feed_frames == 0) {
        ESP_LOGE(tag, "AFE returned an invalid feed size");
        return ESP_ERR_INVALID_SIZE;
    }
    atomic_store(&processor->feed_frames, feed_frames);

    if (!reserve_samples(
            &processor->input_buffer,
            &processor->input_capacity,
            feed_frames * XIAOZHI_AUDIO_INPUT_CHANNELS * 2
        ) ||
            !reserve_samples(
                &processor->output_buffer,
                &processor->output_capacity,
                XIAOZHI_AUDIO_PCM_FRAME_SAMPLES * 2
            )) {
        return ESP_ERR_NO_MEM;
    }

    processor->wake_word_cache_capacity =
        XIAOZHI_AUDIO_WAKE_WORD_CACHE_SAMPLES;
    processor->wake_word_cache = calloc(
        processor->wake_word_cache_capacity,
        sizeof(int16_t)
    );
    if (processor->wake_word_cache == NULL) {
        return ESP_ERR_NO_MEM;
    }
    processor->wake_word_cache_size = 0;
    processor->wake_word_cache_write = 0;
    processor->input_size = 0;
    processor->output_size = 0;
    processor->afe_iface->print_pipeline(processor->afe_data);
    return ESP_OK;
}

static void release_resources(xiaozhi_audio_processor_t *processor)
{
    if (processor == NULL) {
        return;
    }

    atomic_store(&processor->ready, false);
    atomic_store(&processor->feed_frames, 0);
    processor->processing_task = NULL;

    if (lock_mutex(processor->afe_mutex)) {
        if (processor->afe_iface != NULL && processor->afe_data != NULL) {
            processor->afe_iface->destroy(processor->afe_data);
        }
        processor->afe_data = NULL;
        processor->afe_iface = NULL;
        unlock_mutex(processor->afe_mutex);
    }

    if (processor->models != NULL) {
        esp_srmodel_deinit(processor->models);
        processor->models = NULL;
    }
    if (processor->event_group != NULL) {
        vEventGroupDelete(processor->event_group);
        processor->event_group = NULL;
    }

    free(processor->input_buffer);
    processor->input_buffer = NULL;
    processor->input_size = 0;
    processor->input_capacity = 0;
    free(processor->output_buffer);
    processor->output_buffer = NULL;
    processor->output_size = 0;
    processor->output_capacity = 0;
    free(processor->wake_word_cache);
    processor->wake_word_cache = NULL;
    processor->wake_word_cache_size = 0;
    processor->wake_word_cache_write = 0;
    processor->wake_word_cache_capacity = 0;
    free(processor->wakenet_model_name);
    processor->wakenet_model_name = NULL;
    free_wake_words(processor);
    atomic_store(&processor->vad_speaking, false);
}

static void reset_if_inactive(xiaozhi_audio_processor_t *processor)
{
    if (processor == NULL || processor->event_group == NULL ||
            processor->afe_iface == NULL || processor->afe_data == NULL) {
        return;
    }
    EventBits_t bits = xEventGroupGetBits(processor->event_group);
    if ((bits & EVENT_ACTIVE) != 0) {
        return;
    }

    if (lock_mutex(processor->input_mutex)) {
        processor->input_size = 0;
        unlock_mutex(processor->input_mutex);
    }
    processor->afe_iface->reset_buffer(processor->afe_data);
}

static void copy_callbacks(
    xiaozhi_audio_processor_t *processor,
    xiaozhi_audio_processor_callbacks_t *callbacks
)
{
    if (callbacks == NULL) {
        return;
    }
    memset(callbacks, 0, sizeof(*callbacks));
    if (processor == NULL ||
            !lock_mutex(processor->callback_mutex)) {
        return;
    }
    *callbacks = processor->callbacks;
    unlock_mutex(processor->callback_mutex);
}

static void cache_wake_word_pcm(
    xiaozhi_audio_processor_t *processor,
    const afe_fetch_result_t *result
)
{
    if (processor == NULL || result == NULL || result->data == NULL ||
            result->data_size <= 0 || processor->wake_word_cache == NULL) {
        return;
    }

    const int16_t *data = result->data;
    size_t samples = (size_t)result->data_size / sizeof(int16_t);
    if (!lock_mutex(processor->output_mutex)) {
        return;
    }

    size_t capacity = processor->wake_word_cache_capacity;
    if (samples >= capacity) {
        data += samples - capacity;
        memcpy(
            processor->wake_word_cache,
            data,
            capacity * sizeof(int16_t)
        );
        processor->wake_word_cache_size = capacity;
        processor->wake_word_cache_write = 0;
        unlock_mutex(processor->output_mutex);
        return;
    }

    size_t first = samples;
    if (first > capacity - processor->wake_word_cache_write) {
        first = capacity - processor->wake_word_cache_write;
    }
    memcpy(
        processor->wake_word_cache + processor->wake_word_cache_write,
        data,
        first * sizeof(int16_t)
    );
    if (samples > first) {
        memcpy(
            processor->wake_word_cache,
            data + first,
            (samples - first) * sizeof(int16_t)
        );
    }
    processor->wake_word_cache_write =
        (processor->wake_word_cache_write + samples) % capacity;
    processor->wake_word_cache_size += samples;
    if (processor->wake_word_cache_size > capacity) {
        processor->wake_word_cache_size = capacity;
    }
    unlock_mutex(processor->output_mutex);
}

static void handle_pcm(
    xiaozhi_audio_processor_t *processor,
    const afe_fetch_result_t *result
)
{
    if (processor == NULL || result == NULL || result->data == NULL ||
            result->data_size <= 0 || processor->event_group == NULL) {
        return;
    }

    size_t sample_count = (size_t)result->data_size / sizeof(int16_t);
    size_t input_offset = 0;
    while (input_offset < sample_count) {
        bool frame_ready = false;
        if (!lock_mutex(processor->output_mutex)) {
            return;
        }
        if ((xEventGroupGetBits(processor->event_group) & EVENT_SHUTDOWN) != 0) {
            unlock_mutex(processor->output_mutex);
            return;
        }

        size_t required = XIAOZHI_AUDIO_PCM_FRAME_SAMPLES -
                          processor->output_size;
        size_t available = sample_count - input_offset;
        size_t copy_count = required < available ? required : available;
        memcpy(
            processor->output_buffer + processor->output_size,
            result->data + input_offset,
            copy_count * sizeof(int16_t)
        );
        processor->output_size += copy_count;
        input_offset += copy_count;
        if (processor->output_size == XIAOZHI_AUDIO_PCM_FRAME_SAMPLES) {
            memcpy(
                processor->callback_frame,
                processor->output_buffer,
                sizeof(processor->callback_frame)
            );
            processor->output_size = 0;
            frame_ready = true;
        }
        unlock_mutex(processor->output_mutex);

        if (frame_ready) {
            xiaozhi_audio_processor_callbacks_t callbacks;
            copy_callbacks(processor, &callbacks);
            if (callbacks.on_pcm_frame != NULL) {
                callbacks.on_pcm_frame(
                    processor->callback_frame,
                    XIAOZHI_AUDIO_PCM_FRAME_SAMPLES,
                    callbacks.context
                );
            }
        }
    }
}

static void handle_wake_word(
    xiaozhi_audio_processor_t *processor,
    const afe_fetch_result_t *result
)
{
    if (processor == NULL || result == NULL ||
            processor->event_group == NULL) {
        return;
    }
    cache_wake_word_pcm(processor, result);
    if (result->wakeup_state != WAKENET_DETECTED) {
        return;
    }

    const char *wake_word = processor->wakenet_model_name;
    int wake_word_index = result->wake_word_index - 1;
    if (wake_word_index >= 0 &&
            (size_t)wake_word_index < processor->wake_word_count) {
        wake_word = processor->wake_words[wake_word_index];
    } else if (processor->wake_word_count > 0) {
        ESP_LOGW(
            tag,
            "Invalid wake word index %d; using the first wake word",
            result->wake_word_index
        );
        wake_word = processor->wake_words[0];
    }
    if (wake_word == NULL) {
        wake_word = "";
    }

    if (!lock_mutex(processor->afe_mutex)) {
        return;
    }
    if (atomic_load(&processor->shutting_down) ||
            (xEventGroupGetBits(processor->event_group) & EVENT_SHUTDOWN) != 0) {
        unlock_mutex(processor->afe_mutex);
        return;
    }
    xEventGroupClearBits(processor->event_group, EVENT_WAKE_WORD_ENABLED);
    processor->afe_iface->disable_wakenet(processor->afe_data);
    unlock_mutex(processor->afe_mutex);

    ESP_LOGI(tag, "Wake word detected: %s", wake_word);
    xiaozhi_audio_processor_callbacks_t callbacks;
    copy_callbacks(processor, &callbacks);
    if (callbacks.on_wake_word != NULL) {
        callbacks.on_wake_word(wake_word, callbacks.context);
    }
}

static void handle_voice(
    xiaozhi_audio_processor_t *processor,
    const afe_fetch_result_t *result
)
{
    if (processor == NULL || result == NULL ||
            processor->event_group == NULL ||
            (xEventGroupGetBits(processor->event_group) &
            EVENT_VOICE_ENABLED) == 0) {
        return;
    }

    bool speaking = result->vad_state == VAD_SPEECH;
    bool was_speaking = atomic_exchange(
        &processor->vad_speaking,
        speaking
    );
    if (speaking != was_speaking) {
        xiaozhi_audio_processor_callbacks_t callbacks;
        copy_callbacks(processor, &callbacks);
        if (callbacks.on_vad != NULL) {
            callbacks.on_vad(speaking, callbacks.context);
        }
    }
    handle_pcm(processor, result);
}

static void processing_loop(xiaozhi_audio_processor_t *processor)
{
    while (processor != NULL && processor->event_group != NULL) {
        EventBits_t bits = xEventGroupWaitBits(
            processor->event_group,
            EVENT_ACTIVE | EVENT_SHUTDOWN,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY
        );
        if ((bits & EVENT_SHUTDOWN) != 0) {
            break;
        }

        if (processor->afe_iface == NULL || processor->afe_data == NULL) {
            continue;
        }
        afe_fetch_result_t *result = processor->afe_iface->fetch_with_delay(
            processor->afe_data,
            pdMS_TO_TICKS(FETCH_TIMEOUT_MS)
        );
        bits = xEventGroupGetBits(processor->event_group);
        if ((bits & EVENT_SHUTDOWN) != 0) {
            break;
        }
        if (result == NULL || result->ret_value == ESP_FAIL) {
            continue;
        }

        if ((bits & EVENT_WAKE_WORD_ENABLED) != 0) {
            handle_wake_word(processor, result);
        }
        if ((bits & EVENT_VOICE_ENABLED) != 0) {
            handle_voice(processor, result);
        }
    }
}

static void processing_task_entry(void *arg)
{
    xiaozhi_audio_processor_t *processor = arg;
    processing_loop(processor);
    if (processor != NULL && processor->event_group != NULL) {
        xEventGroupSetBits(processor->event_group, EVENT_TASK_EXITED);
    }
    vTaskDelete(NULL);
}

esp_err_t xiaozhi_audio_processor_create(
    const xiaozhi_audio_processor_callbacks_t *callbacks,
    xiaozhi_audio_processor_t **out_processor
)
{
    if (out_processor == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_processor = NULL;
    xiaozhi_audio_processor_t *processor = calloc(1, sizeof(*processor));
    if (processor == NULL) {
        return ESP_ERR_NO_MEM;
    }
    processor->lifecycle_mutex = xSemaphoreCreateMutex();
    processor->afe_mutex = xSemaphoreCreateMutex();
    processor->input_mutex = xSemaphoreCreateMutex();
    processor->output_mutex = xSemaphoreCreateMutex();
    processor->callback_mutex = xSemaphoreCreateMutex();
    if (processor->lifecycle_mutex == NULL || processor->afe_mutex == NULL ||
            processor->input_mutex == NULL || processor->output_mutex == NULL ||
            processor->callback_mutex == NULL) {
        delete_mutexes(processor);
        free(processor);
        return ESP_ERR_NO_MEM;
    }

    if (callbacks != NULL) {
        processor->callbacks = *callbacks;
    }
    atomic_init(&processor->ready, false);
    atomic_init(&processor->shutting_down, false);
    atomic_init(&processor->feed_frames, 0);
    atomic_init(&processor->vad_speaking, false);
    *out_processor = processor;
    return ESP_OK;
}

esp_err_t xiaozhi_audio_processor_destroy(
    xiaozhi_audio_processor_t *processor
)
{
    if (processor == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = xiaozhi_audio_processor_shutdown(processor);
    if (ret != ESP_OK) {
        return ret;
    }
    delete_mutexes(processor);
    free(processor);
    return ESP_OK;
}

esp_err_t xiaozhi_audio_processor_initialize(
    xiaozhi_audio_processor_t *processor
)
{
    if (processor == NULL || processor->lifecycle_mutex == NULL ||
            processor->afe_mutex == NULL || processor->input_mutex == NULL ||
            processor->output_mutex == NULL ||
            processor->callback_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!lock_mutex(processor->lifecycle_mutex)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (atomic_load(&processor->ready)) {
        unlock_mutex(processor->lifecycle_mutex);
        return ESP_OK;
    }
    if (atomic_load(&processor->shutting_down)) {
        unlock_mutex(processor->lifecycle_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    release_resources(processor);
    processor->event_group = xEventGroupCreate();
    if (processor->event_group == NULL) {
        unlock_mutex(processor->lifecycle_mutex);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = create_afe(processor);
    if (ret != ESP_OK) {
        release_resources(processor);
        unlock_mutex(processor->lifecycle_mutex);
        return ret;
    }
    xEventGroupClearBits(
        processor->event_group,
        EVENT_WAKE_WORD_ENABLED | EVENT_VOICE_ENABLED |
        EVENT_SHUTDOWN | EVENT_TASK_EXITED
    );

    BaseType_t created = xTaskCreate(
        processing_task_entry,
        "xiaozhi_afe",
        PROCESS_TASK_STACK_SIZE,
        processor,
        PROCESS_TASK_PRIORITY,
        &processor->processing_task
    );
    if (created != pdPASS) {
        processor->processing_task = NULL;
        release_resources(processor);
        unlock_mutex(processor->lifecycle_mutex);
        return ESP_ERR_NO_MEM;
    }

    atomic_store(&processor->ready, true);
    ESP_LOGI(
        tag,
        "Ready: input=%s, feed=%u frames, output=%u samples",
        afe_input_format,
        (unsigned)atomic_load(&processor->feed_frames),
        (unsigned)XIAOZHI_AUDIO_PCM_FRAME_SAMPLES
    );
    unlock_mutex(processor->lifecycle_mutex);
    return ESP_OK;
}

esp_err_t xiaozhi_audio_processor_shutdown(
    xiaozhi_audio_processor_t *processor
)
{
    if (processor == NULL || processor->lifecycle_mutex == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!lock_mutex(processor->lifecycle_mutex)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (processor->event_group == NULL) {
        atomic_store(&processor->ready, false);
        atomic_store(&processor->shutting_down, false);
        unlock_mutex(processor->lifecycle_mutex);
        return ESP_OK;
    }
    if (processor->processing_task != NULL &&
            xTaskGetCurrentTaskHandle() == processor->processing_task) {
        ESP_LOGE(tag, "shutdown() cannot be called from an audio callback");
        unlock_mutex(processor->lifecycle_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    bool task_running = processor->processing_task != NULL;
    atomic_store(&processor->shutting_down, true);
    atomic_store(&processor->ready, false);
    xEventGroupClearBits(
        processor->event_group,
        EVENT_WAKE_WORD_ENABLED | EVENT_VOICE_ENABLED
    );
    xEventGroupSetBits(processor->event_group, EVENT_SHUTDOWN);

    if (lock_mutex(processor->afe_mutex)) {
        if (processor->afe_iface != NULL && processor->afe_data != NULL) {
            processor->afe_iface->disable_wakenet(processor->afe_data);
        }
        unlock_mutex(processor->afe_mutex);
    }

    if (task_running) {
        xEventGroupWaitBits(
            processor->event_group,
            EVENT_TASK_EXITED,
            pdFALSE,
            pdTRUE,
            portMAX_DELAY
        );
        vTaskDelay(1);
    }
    if (lock_mutex(processor->input_mutex)) {
        processor->input_size = 0;
        unlock_mutex(processor->input_mutex);
    }
    if (lock_mutex(processor->output_mutex)) {
        processor->output_size = 0;
        unlock_mutex(processor->output_mutex);
    }

    release_resources(processor);
    atomic_store(&processor->shutting_down, false);
    unlock_mutex(processor->lifecycle_mutex);
    return ESP_OK;
}

esp_err_t xiaozhi_audio_processor_feed(
    xiaozhi_audio_processor_t *processor,
    const int16_t *interleaved_pcm,
    size_t frame_count
)
{
    if (processor == NULL || interleaved_pcm == NULL || frame_count == 0 ||
            !atomic_load(&processor->ready) ||
            atomic_load(&processor->shutting_down)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!lock_mutex(processor->input_mutex)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!atomic_load(&processor->ready) ||
            atomic_load(&processor->shutting_down) ||
            processor->event_group == NULL || processor->afe_iface == NULL ||
            processor->afe_data == NULL) {
        unlock_mutex(processor->input_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    EventBits_t bits = xEventGroupGetBits(processor->event_group);
    if ((bits & EVENT_ACTIVE) == 0 || (bits & EVENT_SHUTDOWN) != 0) {
        unlock_mutex(processor->input_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    if (frame_count > SIZE_MAX / XIAOZHI_AUDIO_INPUT_CHANNELS) {
        unlock_mutex(processor->input_mutex);
        return ESP_ERR_INVALID_SIZE;
    }
    size_t sample_count = frame_count * XIAOZHI_AUDIO_INPUT_CHANNELS;
    if (sample_count > SIZE_MAX - processor->input_size ||
            !reserve_samples(
                &processor->input_buffer,
                &processor->input_capacity,
                processor->input_size + sample_count
            )) {
        unlock_mutex(processor->input_mutex);
        return ESP_ERR_NO_MEM;
    }
    memcpy(
        processor->input_buffer + processor->input_size,
        interleaved_pcm,
        sample_count * sizeof(int16_t)
    );
    processor->input_size += sample_count;

    size_t feed_frames = atomic_load(&processor->feed_frames);
    size_t feed_samples = feed_frames * XIAOZHI_AUDIO_INPUT_CHANNELS;
    if (feed_samples == 0) {
        unlock_mutex(processor->input_mutex);
        return ESP_ERR_INVALID_SIZE;
    }

    size_t consumed = 0;
    while (processor->input_size - consumed >= feed_samples) {
        processor->afe_iface->feed(
            processor->afe_data,
            processor->input_buffer + consumed
        );
        consumed += feed_samples;
    }
    if (consumed > 0) {
        memmove(
            processor->input_buffer,
            processor->input_buffer + consumed,
            (processor->input_size - consumed) * sizeof(int16_t)
        );
        processor->input_size -= consumed;
    }
    unlock_mutex(processor->input_mutex);
    return ESP_OK;
}

esp_err_t xiaozhi_audio_processor_enable_wake_word(
    xiaozhi_audio_processor_t *processor,
    bool enable
)
{
    if (processor == NULL || !lock_mutex(processor->afe_mutex)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!atomic_load(&processor->ready) ||
            atomic_load(&processor->shutting_down) ||
            processor->event_group == NULL || processor->afe_iface == NULL ||
            processor->afe_data == NULL) {
        unlock_mutex(processor->afe_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    EventBits_t bits = xEventGroupGetBits(processor->event_group);
    bool enabled = (bits & EVENT_WAKE_WORD_ENABLED) != 0;
    if (enabled == enable) {
        unlock_mutex(processor->afe_mutex);
        return ESP_OK;
    }

    if (enable) {
        if (lock_mutex(processor->output_mutex)) {
            processor->wake_word_cache_size = 0;
            processor->wake_word_cache_write = 0;
            unlock_mutex(processor->output_mutex);
        }
        processor->afe_iface->enable_wakenet(processor->afe_data);
        xEventGroupSetBits(processor->event_group, EVENT_WAKE_WORD_ENABLED);
    } else {
        xEventGroupClearBits(processor->event_group, EVENT_WAKE_WORD_ENABLED);
        processor->afe_iface->disable_wakenet(processor->afe_data);
        reset_if_inactive(processor);
    }
    unlock_mutex(processor->afe_mutex);
    return ESP_OK;
}

esp_err_t xiaozhi_audio_processor_enable_voice_processing(
    xiaozhi_audio_processor_t *processor,
    bool enable
)
{
    if (processor == NULL || !lock_mutex(processor->afe_mutex)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!atomic_load(&processor->ready) ||
            atomic_load(&processor->shutting_down) ||
            processor->event_group == NULL || processor->afe_iface == NULL ||
            processor->afe_data == NULL) {
        unlock_mutex(processor->afe_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    EventBits_t bits = xEventGroupGetBits(processor->event_group);
    bool enabled = (bits & EVENT_VOICE_ENABLED) != 0;
    if (enabled == enable) {
        unlock_mutex(processor->afe_mutex);
        return ESP_OK;
    }

    if (lock_mutex(processor->output_mutex)) {
        processor->output_size = 0;
        atomic_store(&processor->vad_speaking, false);
        unlock_mutex(processor->output_mutex);
    }
    if (enable) {
        xEventGroupSetBits(processor->event_group, EVENT_VOICE_ENABLED);
    } else {
        xEventGroupClearBits(processor->event_group, EVENT_VOICE_ENABLED);
        reset_if_inactive(processor);
    }
    unlock_mutex(processor->afe_mutex);
    return ESP_OK;
}

bool xiaozhi_audio_processor_is_ready(
    const xiaozhi_audio_processor_t *processor
)
{
    return processor != NULL && atomic_load(&processor->ready) &&
           !atomic_load(&processor->shutting_down);
}

size_t xiaozhi_audio_processor_get_feed_frames(
    const xiaozhi_audio_processor_t *processor
)
{
    return processor != NULL ? atomic_load(&processor->feed_frames) : 0;
}

esp_err_t xiaozhi_audio_processor_take_wake_word_pcm(
    xiaozhi_audio_processor_t *processor,
    int16_t *pcm,
    size_t capacity,
    size_t *sample_count
)
{
    if (processor == NULL || sample_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *sample_count = 0;
    if (!lock_mutex(processor->output_mutex)) {
        return ESP_ERR_INVALID_STATE;
    }
    size_t required = processor->wake_word_cache_size;
    if (required == 0) {
        unlock_mutex(processor->output_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    *sample_count = required;
    if (pcm == NULL || capacity < required) {
        unlock_mutex(processor->output_mutex);
        return ESP_ERR_INVALID_SIZE;
    }

    size_t capacity_cache = processor->wake_word_cache_capacity;
    size_t oldest = (
        processor->wake_word_cache_write + capacity_cache - required
    ) % capacity_cache;
    size_t first = required;
    if (first > capacity_cache - oldest) {
        first = capacity_cache - oldest;
    }
    memcpy(pcm, processor->wake_word_cache + oldest, first * sizeof(int16_t));
    if (required > first) {
        memcpy(
            pcm + first,
            processor->wake_word_cache,
            (required - first) * sizeof(int16_t)
        );
    }
    processor->wake_word_cache_size = 0;
    processor->wake_word_cache_write = 0;
    unlock_mutex(processor->output_mutex);
    return ESP_OK;
}

esp_err_t xiaozhi_audio_processor_set_callbacks(
    xiaozhi_audio_processor_t *processor,
    const xiaozhi_audio_processor_callbacks_t *callbacks
)
{
    if (processor == NULL || callbacks == NULL ||
            !lock_mutex(processor->callback_mutex)) {
        return ESP_ERR_INVALID_ARG;
    }
    processor->callbacks = *callbacks;
    unlock_mutex(processor->callback_mutex);
    return ESP_OK;
}
