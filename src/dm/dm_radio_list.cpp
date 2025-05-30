/**
 * Copyright 2023 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/filter.h>
#include <netinet/ether.h>
#include <netpacket/packet.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include "em_cmd.h"
#include "dm_radio_list.h"
#include "dm_easy_mesh.h"
#include "dm_easy_mesh_ctrl.h"

int dm_radio_list_t::get_config(cJSON *obj_arr, void *parent, bool summary)
{
	return 0;
}

int dm_radio_list_t::get_config(cJSON *obj_arr, void *parent, em_get_radio_list_reason_t reason)
{
    dm_radio_t *pradio;
    cJSON *obj;
    mac_addr_str_t  mac_str;
    char *dev_mac_str = static_cast<char *>(parent);
    mac_address_t	dev_mac;

    dm_easy_mesh_t::string_to_macbytes(dev_mac_str, dev_mac);

    pradio = get_first_radio();
    while (pradio != NULL) {
        dm_easy_mesh_t::macbytes_to_string(pradio->m_radio_info.intf.mac, mac_str);
        if (memcmp(dev_mac, pradio->m_radio_info.id.dev_mac, sizeof(mac_address_t)) != 0) {
            pradio = get_next_radio(pradio);
            continue;
        }
        obj = cJSON_CreateObject();

        pradio->encode(obj, reason);
        cJSON_AddItemToArray(obj_arr, obj);
        pradio = get_next_radio(pradio);
    }

    return 0;
}

int dm_radio_list_t::set_config(db_client_t& db_client, dm_radio_t& radio, void *parent_id)
{
    dm_orch_type_t op;
    
    dm_radio_t::parse_radio_id_from_key(static_cast<char *>(parent_id), &radio.m_radio_info.id);
    
    update_db(db_client, (op = get_dm_orch_type(db_client, radio)), radio.get_radio_info());
    update_list(radio, op);
    
    return 0;
}

int dm_radio_list_t::set_config(db_client_t& db_client, const cJSON *obj_arr, void *parent_id)
{
    cJSON *obj;
    int i, size;
    dm_radio_t radio;
    dm_orch_type_t op;

    size = cJSON_GetArraySize(obj_arr);

    for (i = 0; i < size; i++) {
        obj = cJSON_GetArrayItem(obj_arr, i);
		radio.decode(obj, parent_id);
		update_db(db_client, (op = get_dm_orch_type(db_client, radio)), radio.get_radio_info());
		update_list(radio, op);
    }

    return 0;
}

dm_orch_type_t dm_radio_list_t::get_dm_orch_type(db_client_t& db_client, const dm_radio_t& radio)
{
    dm_radio_t *pradio;
    mac_addr_str_t  mac_str = {0};
    mac_addr_str_t radio_mac_str, dev_mac_str;
    em_2xlong_string_t key;

    dm_easy_mesh_t::macbytes_to_string(const_cast<unsigned char *>(radio.m_radio_info.id.dev_mac), dev_mac_str);
    dm_easy_mesh_t::macbytes_to_string(const_cast<unsigned char *>(radio.m_radio_info.id.ruid), radio_mac_str);
    snprintf(key, sizeof(key), "%s@%s@%s", radio.m_radio_info.id.net_id, dev_mac_str, radio_mac_str);

    dm_easy_mesh_t::macbytes_to_string(const_cast<unsigned char *>(radio.m_radio_info.intf.mac), mac_str);
    pradio = get_radio(mac_str);
    if (pradio != NULL) {
        if (entry_exists_in_table(db_client, key) == false) {
            return dm_orch_type_db_insert;
        }

        if (*pradio == radio) {
            printf("%s:%d: Device: %s already in list\n", __func__, __LINE__,
                        dm_easy_mesh_t::macbytes_to_string(pradio->m_radio_info.intf.mac, mac_str));
            return dm_orch_type_none;
        }


        printf("%s:%d: Radio: %s in list but needs update\n", __func__, __LINE__,
        		dm_easy_mesh_t::macbytes_to_string(pradio->m_radio_info.intf.mac, mac_str));
        return dm_orch_type_db_update;
    }  

    return dm_orch_type_db_insert;

}

void dm_radio_list_t::update_list(const dm_radio_t& radio, dm_orch_type_t op)
{
    dm_radio_t *pradio;
    mac_addr_str_t	mac_str = {0};

    dm_easy_mesh_t::macbytes_to_string(const_cast<unsigned char *>(radio.m_radio_info.intf.mac), mac_str);

    switch (op) {
        case dm_orch_type_db_insert:
            put_radio(mac_str, &radio);
            break;

        case dm_orch_type_db_update:
            pradio = get_radio(mac_str);
            memcpy(&pradio->m_radio_info, &radio.m_radio_info, sizeof(em_radio_info_t));
            break;

        case dm_orch_type_db_delete:
            remove_radio(mac_str);
            break;

        default:
            break;
    }

}

void dm_radio_list_t::delete_list()
{       
    dm_radio_t *pradio, *tmp;
    mac_addr_str_t	mac_str = {0};
    
    pradio = get_first_radio();
    while (pradio != NULL) {
        tmp = pradio;
        pradio = get_next_radio(pradio);       
        dm_easy_mesh_t::macbytes_to_string(const_cast<unsigned char *>(tmp->m_radio_info.intf.mac), mac_str);
        remove_radio(mac_str);    
    }
}   


bool dm_radio_list_t::operator == (const db_easy_mesh_t& obj)
{
    return true;
}

int dm_radio_list_t::update_db(db_client_t& db_client, dm_orch_type_t op, void *data)
{
    mac_addr_str_t radio_mac_str, dev_mac_str;
    em_radio_info_t *info = static_cast<em_radio_info_t *>(data);
    int ret = 0;
	em_2xlong_string_t key;

    dm_easy_mesh_t::macbytes_to_string(const_cast<unsigned char *>(info->id.dev_mac), dev_mac_str);
    dm_easy_mesh_t::macbytes_to_string(const_cast<unsigned char *>(info->id.ruid), radio_mac_str);
	snprintf(key, sizeof(key), "%s@%s@%s", info->id.net_id, dev_mac_str, radio_mac_str);

    switch (op) {
		case dm_orch_type_db_insert:
			ret = insert_row(db_client, key, radio_mac_str, info->enabled,
						info->media_data.media_type, info->band, info->media_data.center_freq_index_1, info->media_data.center_freq_index_1,
            			info->number_of_bss, info->number_of_unassoc_sta, info->noise, info->utilization, 
						info->traffic_sep_combined_fronthaul, 
						info->traffic_sep_combined_backhaul, info->steering_policy, info->channel_util_threshold, info->rcpi_steering_threshold, 
            			info->sta_reporting_rcpi_threshold, info->sta_reporting_hysteresis_margin_override, info->channel_utilization_reporting_threshold, 
						info->associated_sta_traffic_stats_inclusion_policy, info->associated_sta_link_mterics_inclusion_policy,
            			info->chip_vendor); 
			break;

		case dm_orch_type_db_update:
			ret = update_row(db_client, radio_mac_str, info->enabled,
                        info->media_data.media_type, info->band, info->media_data.center_freq_index_1, info->media_data.center_freq_index_1,
                        info->number_of_bss, info->number_of_unassoc_sta, info->noise, info->utilization,
                        info->traffic_sep_combined_fronthaul,
                        info->traffic_sep_combined_backhaul, info->steering_policy, info->channel_util_threshold, info->rcpi_steering_threshold,
                        info->sta_reporting_rcpi_threshold, info->sta_reporting_hysteresis_margin_override, info->channel_utilization_reporting_threshold, 
                        info->associated_sta_traffic_stats_inclusion_policy, info->associated_sta_link_mterics_inclusion_policy,
                        info->chip_vendor, key);
			break;

		case dm_orch_type_db_delete:
			ret = delete_row(db_client, key);
			break;

		default:
			break;
	}

    return ret;
}

bool dm_radio_list_t::search_db(db_client_t& db_client, void *ctx, void *key)
{
    em_long_string_t  str;

    while (db_client.next_result(ctx)) {
        db_client.get_string(ctx, str, 1);

        if (strncmp(str, static_cast<char *>(key), strlen(static_cast<char *>(key))) == 0) {
            return true;
        }
    }

    return false;
}

int dm_radio_list_t::sync_db(db_client_t& db_client, void *ctx)
{
    em_radio_info_t info;
    mac_addr_str_t	mac;
    em_long_string_t   str;
    int rc = 0;

    while (db_client.next_result(ctx)) {
        memset(&info, 0, sizeof(em_radio_info_t));

        db_client.get_string(ctx, str, 1);
		dm_radio_t::parse_radio_id_from_key(str, &info.id);

        db_client.get_string(ctx, mac, 2);
        dm_easy_mesh_t::string_to_macbytes(mac, info.intf.mac);

        info.enabled = db_client.get_number(ctx, 3);
        info.media_data.media_type = static_cast<short unsigned int>(db_client.get_number(ctx, 4));
        info.media_data.band = static_cast<unsigned char>(db_client.get_number(ctx, 5));
        info.band = static_cast<em_freq_band_t> (db_client.get_number(ctx, 5));
        info.media_data.center_freq_index_1 = static_cast<unsigned char>(db_client.get_number(ctx, 6));
        info.media_data.center_freq_index_2 = static_cast<unsigned char>(db_client.get_number(ctx, 7));
        info.number_of_bss = static_cast<unsigned int>(db_client.get_number(ctx, 8));
        info.number_of_unassoc_sta = static_cast<unsigned int>(db_client.get_number(ctx, 9));
        info.noise = db_client.get_number(ctx, 10);
        info.utilization = static_cast<short unsigned int>(db_client.get_number(ctx, 11));
        info.traffic_sep_combined_fronthaul = db_client.get_number(ctx, 12);
        info.traffic_sep_combined_backhaul = db_client.get_number(ctx, 13);
        info.steering_policy = static_cast<unsigned int>(db_client.get_number(ctx, 14));
        info.channel_util_threshold = static_cast<unsigned int>(db_client.get_number(ctx, 15));
        info.rcpi_steering_threshold = static_cast<unsigned int>(db_client.get_number(ctx, 16));
        info.sta_reporting_rcpi_threshold = static_cast<unsigned int>(db_client.get_number(ctx, 17));
        info.sta_reporting_hysteresis_margin_override = static_cast<unsigned int>(db_client.get_number(ctx, 18));
        info.channel_utilization_reporting_threshold = static_cast<unsigned int>(db_client.get_number(ctx, 19));
        info.associated_sta_traffic_stats_inclusion_policy = db_client.get_number(ctx, 20);
        info.associated_sta_link_mterics_inclusion_policy = db_client.get_number(ctx, 21);

        db_client.get_string(ctx, info.chip_vendor, 22);

        update_list(dm_radio_t(&info), dm_orch_type_db_insert);
    }
    return rc;
}

void dm_radio_list_t::init_table()
{
    snprintf(m_table_name, sizeof(m_table_name), "%s", "RadioList");
}

void dm_radio_list_t::init_columns()
{
    m_num_cols = 0;

    m_columns[m_num_cols++] = db_column_t("ID", db_data_type_char, 128);
    m_columns[m_num_cols++] = db_column_t("RadioID", db_data_type_char, 17);
    m_columns[m_num_cols++] = db_column_t("Enabled", db_data_type_tinyint, 0);
    m_columns[m_num_cols++] = db_column_t("MediaType", db_data_type_int, 0);
    m_columns[m_num_cols++] = db_column_t("Band", db_data_type_tinyint, 0);
    m_columns[m_num_cols++] = db_column_t("CF1", db_data_type_int, 0);
    m_columns[m_num_cols++] = db_column_t("CF2", db_data_type_int, 0);
    m_columns[m_num_cols++] = db_column_t("NumberOfBSS", db_data_type_tinyint, 0);
    m_columns[m_num_cols++] = db_column_t("NumberOfUnassocSta", db_data_type_tinyint, 0);
    m_columns[m_num_cols++] = db_column_t("Noise", db_data_type_tinyint, 0);
    m_columns[m_num_cols++] = db_column_t("Utilization", db_data_type_tinyint, 0);
    m_columns[m_num_cols++] = db_column_t("TrafficSeparationCombinedFronthaul", db_data_type_tinyint, 0);
    m_columns[m_num_cols++] = db_column_t("TrafficSeparationCombinedBackhaul", db_data_type_tinyint, 0);
    m_columns[m_num_cols++] = db_column_t("SteeringPolicy", db_data_type_tinyint, 0);
    m_columns[m_num_cols++] = db_column_t("ChannelUtilizationThreshold", db_data_type_smallint, 0);
    m_columns[m_num_cols++] = db_column_t("RCPISteeringThreshold", db_data_type_smallint, 0);
    m_columns[m_num_cols++] = db_column_t("STAReportingRCPIThreshold", db_data_type_smallint, 0);
    m_columns[m_num_cols++] = db_column_t("STAReportingRCPIHysteresisMarginOverride", db_data_type_tinyint, 0);
    m_columns[m_num_cols++] = db_column_t("ChannelUtilizationReportingThreshold", db_data_type_smallint, 0);
    m_columns[m_num_cols++] = db_column_t("AssociatedSTATrafficStatsInclusionPolicy", db_data_type_tinyint, 0);
    m_columns[m_num_cols++] = db_column_t("AssociatedSTALinkMetricsInclusionPolicy", db_data_type_tinyint, 0);
    m_columns[m_num_cols++] = db_column_t("ChipsetVendor", db_data_type_char, 64);
}

int dm_radio_list_t::init()
{
    init_table();
    init_columns();
    return 0;
}
