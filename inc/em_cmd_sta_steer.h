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

#ifndef EM_CMD_STA_STEER_H
#define EM_CMD_STA_STEER_H

#include "em_cmd.h"

class em_cmd_sta_steer_t : public em_cmd_t {

public:
    
	/**!
	 * @brief 
	 *
	 * This function is responsible for handling the steering of STA (Station) based on the provided parameters.
	 *
	 * @param[in] params The parameters required for steering the STA.
	 *
	 * @returns em_cmd_sta_steer_t
	 *
	 * @note Ensure that the parameters are correctly initialized before calling this function.
	 */
	em_cmd_sta_steer_t(em_cmd_steer_params_t params);
};

#endif
