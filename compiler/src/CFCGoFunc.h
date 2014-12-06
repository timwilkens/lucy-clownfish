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


#ifndef H_CFCGOFUNC
#define H_CFCGOFUNC

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CFCGoFunc CFCGoFunc;
struct CFCParcel;
struct CFCFunction;
struct CFCParamList;
struct CFCType;

#ifdef CFC_NEED_GOFUNC_STRUCT_DEF
#define CFC_NEED_BASE_STRUCT_DEF
#include "CFCBase.h"
struct CFCGoFunc {
    CFCBase base;
    struct CFCParcel *parcel;
    struct CFCParamList *param_list;
    char *go_name;
    char *c_name;
};
#endif

#if 0
/** Clownfish::CFC::Binding::Go::Function
 * 
 * This class is used to generate binding code for invoking Clownfish's
 * inert functions the Go/C barrier.
 */ 

/** Constructor.
 * 
 * @param parcel The Clownfish parcel the function belongs to.
 * @param param_list A Clownfish::CFC::Model::ParamList.
 */
CFCGoFunc*
CFCGoFunc_init(CFCGoFunc *self, struct CFCParcel *parcel,
               struct CFCParamList *param_list);

void
CFCGoFunc_destroy(CFCGoFunc *self);
#endif

/** Test whether bindings can be generated for a CFCFunction.
  */
int
CFCGoFunc_can_be_bound(struct CFCFunction *function);

#if 0
/** Generate C declarations for the variables holding the arguments.
 */
char*
CFCGoFunc_arg_declarations(CFCGoFunc *self);

/** Create a comma-separated list of argument names prefixed by "arg_".
 */
char*
CFCGoFunc_arg_name_list(CFCGoFunc *self);

/** Generate code which will invoke XSBind_allot_params() to parse labeled
 * parameters supplied to an XSUB.
 */
char*
CFCGoFunc_build_allot_params(CFCGoFunc *self);

/** Accessor for param list.
 */
struct CFCParamList*
CFCGoFunc_get_param_list(CFCGoFunc *self);

/**
 * @return the fully-qualified perl subroutine name.
 */
const char*
CFCGoFunc_perl_name(CFCGoFunc *self);

const char*
CFCGoFunc_c_name(CFCGoFunc *self);

/**
 * @return a string containing the names of arguments to feed to bound C
 * function, joined by commas.
 */
const char*
CFCGoFunc_c_name_list(CFCGoFunc *self);

#ifdef __cplusplus
}
#endif

#endif /* H_CFCGOFUNC */

#endif
