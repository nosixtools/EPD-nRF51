#include <string.h>
#include "nordic_common.h"
#include "fds.h"
#include "EPD_config.h"
#include "nrf_log.h"

#define CONFIG_FILE_ID 0x0000
#define CONFIG_REC_KEY 0x0001

static void fds_evt_handler(fds_evt_t const * const p_fds_evt)
{
    NRF_LOG_DEBUG("fds evt: id=%d result=%d\n", p_fds_evt->id, p_fds_evt->result);
}

void epd_config_init(epd_config_t *cfg)
{
    ret_code_t ret;

    ret = fds_register(fds_evt_handler);
    if (ret != NRF_SUCCESS) {
        NRF_LOG_ERROR("fds_register failed!\n");
        return;
    }

    ret = fds_init();
    if (ret != NRF_SUCCESS) {
        NRF_LOG_ERROR("fds init failed!\n");
    }
}

void epd_config_load(epd_config_t *cfg)
{
    fds_flash_record_t  flash_record;
    fds_record_desc_t   record_desc;
    fds_find_token_t    ftok;

    memset(cfg, EPD_CONFIG_EMPTY, sizeof(epd_config_t));
    memset(&ftok, 0x00, sizeof(fds_find_token_t));

    if (fds_record_find(CONFIG_FILE_ID, CONFIG_REC_KEY, &record_desc, &ftok) != NRF_SUCCESS) {
        NRF_LOG_DEBUG("epd_config_load: record not found\n");
        return;
    }
    if (fds_record_open(&record_desc, &flash_record) != NRF_SUCCESS) {
        NRF_LOG_ERROR("epd_config_load: record open failed!");
        return;
    }
#ifdef S112
    uint32_t record_len = flash_record.p_header->length_words * sizeof(uint32_t);
#else
    uint32_t record_len = flash_record.p_header->tl.length_words * sizeof(uint32_t);
#endif
    memcpy(cfg, flash_record.p_data, MIN(sizeof(epd_config_t), record_len));
    fds_record_close(&record_desc);
}

void epd_config_clear(epd_config_t *cfg)
{
    fds_record_desc_t   record_desc;
    fds_find_token_t    ftok;

    memset(&ftok, 0x00, sizeof(fds_find_token_t));
    if (fds_record_find(CONFIG_FILE_ID, CONFIG_REC_KEY, &record_desc, &ftok) != NRF_SUCCESS) {
        NRF_LOG_DEBUG("epd_config_clear: record not found\n");
        return;
    }

    fds_record_delete(&record_desc);
}

void epd_config_save(epd_config_t *cfg)
{
    ret_code_t          ret;
    fds_record_t        record;
    fds_record_desc_t   record_desc;
    fds_find_token_t    ftok;

    record.file_id = CONFIG_FILE_ID;
    record.key = CONFIG_REC_KEY;
#ifdef S112
    record.data.p_data = (void*)cfg;
    record.data.length_words = BYTES_TO_WORDS(sizeof(epd_config_t));
#else
    fds_record_chunk_t record_chunk;
    record_chunk.p_data = cfg;
    record_chunk.length_words = BYTES_TO_WORDS(sizeof(epd_config_t));
    record.data.p_chunks = &record_chunk;
    record.data.num_chunks = 1;
#endif

    memset(&ftok, 0x00, sizeof(fds_find_token_t));
    ret = fds_record_find(CONFIG_FILE_ID, CONFIG_REC_KEY, &record_desc, &ftok);
    if (ret == NRF_SUCCESS)
        ret = fds_record_update(&record_desc, &record);
    else
        ret = fds_record_write(&record_desc, &record);

    if (ret != NRF_SUCCESS) {
        NRF_LOG_ERROR("epd_config_save: record write/update failed!\n");
    }
}

bool epd_config_empty(epd_config_t *cfg)
{
    for (uint8_t i = 0; i < EPD_CONFIG_SIZE; i++) {
        if (((uint8_t *)cfg)[i] != EPD_CONFIG_EMPTY)
            return false;
    }
    return true;
}
