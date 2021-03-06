/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef H_CFCURI
#define H_CFCURI

#ifdef __cplusplus
extern "C" {
#endif

#define CFC_URI_PARCEL      1
#define CFC_URI_CLASS       2
#define CFC_URI_FUNCTION    3
#define CFC_URI_METHOD      4
#define CFC_URI_NULL        5

typedef struct CFCUri CFCUri;
struct CFCClass;

int
CFCUri_is_clownfish_uri(const char *uri);

CFCUri*
CFCUri_new(const char *uri, struct CFCClass *klass);

CFCUri*
CFCUri_init(CFCUri *self, const char *uri, struct CFCClass *klass);

void
CFCUri_destroy(CFCUri *self);

int
CFCUri_get_type(CFCUri *self);

const char*
CFCUri_get_prefix(CFCUri *self);

const char*
CFCUri_get_struct_sym(CFCUri *self);

const char*
CFCUri_full_struct_sym(CFCUri *self);

const char*
CFCUri_get_func_sym(CFCUri *self);

#ifdef __cplusplus
}
#endif

#endif /* H_CFCURI */

