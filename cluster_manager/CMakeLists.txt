# Copyright 2016 Husky Team
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
# http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


include_directories(${PROJECT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/husky ${HUSKY_EXTERNAL_INCLUDE})

add_subdirectory(task_scheduler)

file(GLOB mlmanager-src-files
    cluster_manager.cpp
    scheduler_trigger.cpp)
husky_cache_variable(mlmanager-src-files ${mlmanager-src-files})

add_library(mlmanager-objs OBJECT ${mlmanager-src-files})
husky_default_properties(mlmanager-objs)
