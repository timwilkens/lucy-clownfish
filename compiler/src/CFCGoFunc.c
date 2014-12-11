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


#include <string.h>
#include <stdio.h>
#define CFC_NEED_BASE_STRUCT_DEF
#define CFC_NEED_GOFUNC_STRUCT_DEF
#include "CFCGoFunc.h"
#include "CFCBase.h"
#include "CFCFunction.h"
#include "CFCUtil.h"
#include "CFCParamList.h"
#include "CFCVariable.h"
#include "CFCType.h"

#ifndef true
    #define true 1
    #define false 0
#endif
#if 0

CFCPerlSub*
CFCPerlSub_init(CFCPerlSub *self, CFCParamList *param_list,
                const char *class_name, const char *alias,
                int use_labeled_params) {
    CFCUTIL_NULL_CHECK(param_list);
    CFCUTIL_NULL_CHECK(class_name);
    CFCUTIL_NULL_CHECK(alias);
    self->param_list  = (CFCParamList*)CFCBase_incref((CFCBase*)param_list);
    self->class_name  = CFCUtil_strdup(class_name);
    self->alias       = CFCUtil_strdup(alias);
    self->use_labeled_params = use_labeled_params;
    self->perl_name = CFCUtil_sprintf("%s::%s", class_name, alias);

    size_t c_name_len = strlen(self->perl_name) + sizeof("XS_") + 1;
    self->c_name = (char*)MALLOCATE(c_name_len);
    int j = 3;
    memcpy(self->c_name, "XS_", j);
    for (int i = 0, max = (int)strlen(self->perl_name); i < max; i++) {
        char c = self->perl_name[i];
        if (c == ':') {
            while (self->perl_name[i + 1] == ':') { i++; }
            self->c_name[j++] = '_';
        }
        else {
            self->c_name[j++] = c;
        }
    }
    self->c_name[j] = 0; // NULL-terminate.

    return self;
}

void
CFCPerlSub_destroy(CFCPerlSub *self) {
    CFCBase_decref((CFCBase*)self->param_list);
    FREEMEM(self->class_name);
    FREEMEM(self->alias);
    FREEMEM(self->perl_name);
    FREEMEM(self->c_name);
    CFCBase_destroy((CFCBase*)self);
}
#endif

int
CFCGoFunc_can_be_bound(CFCFunction *function) {
    // Test whether parameters can be mapped automatically.
    CFCParamList  *param_list = CFCFunction_get_param_list(function);
    CFCVariable  **arg_vars   = CFCParamList_get_variables(param_list);
    for (size_t i = 0; arg_vars[i] != NULL; i++) {
        CFCType *type = CFCVariable_get_type(arg_vars[i]);
        if (!CFCType_is_object(type) && !CFCType_is_primitive(type)) {
            return false;
        }
    }

    // Test whether return type can be mapped automatically.
    CFCType *return_type = CFCFunction_get_return_type(function);
    if (!CFCType_is_void(return_type)
        && !CFCType_is_object(return_type)
        && !CFCType_is_primitive(return_type)
    ) {
        return false;
    }

    return true;
}

#if 0
char*
CFCPerlSub_params_hash_def(CFCPerlSub *self) {
    if (!self->use_labeled_params) {
        return NULL;
    }

    char *def = CFCUtil_strdup("");
    def = CFCUtil_cat(def, "%", self->perl_name, "_PARAMS = (", NULL);

    CFCVariable **arg_vars = CFCParamList_get_variables(self->param_list);
    const char **vals = CFCParamList_get_initial_values(self->param_list);

    // No labeled params means an empty params hash def.
    if (!arg_vars[1]) {
        def = CFCUtil_cat(def, ");\n", NULL);
        return def;
    }

    for (int i = 1; arg_vars[i] != NULL; i++) {
        CFCVariable *var = arg_vars[i];
        const char *micro_sym = CFCVariable_micro_sym(var);
        const char *val = vals[i];
        val = val == NULL
              ? "undef"
              : strcmp(val, "NULL") == 0
              ? "undef"
              : strcmp(val, "true") == 0
              ? "1"
              : strcmp(val, "false") == 0
              ? "0"
              : val;
        def = CFCUtil_cat(def, "\n    ", micro_sym, " => ", val, ",", NULL);
    }
    def = CFCUtil_cat(def, "\n);\n", NULL);

    return def;
}

struct allot_macro_map {
    const char *prim_type;
    const char *allot_macro;
};

struct allot_macro_map prim_type_to_allot_macro[] = {
    { "double",     "ALLOT_F64"    },
    { "float",      "ALLOT_F32"    },
    { "int",        "ALLOT_INT"    },
    { "short",      "ALLOT_SHORT"  },
    { "long",       "ALLOT_LONG"   },
    { "size_t",     "ALLOT_SIZE_T" },
    { "uint64_t",   "ALLOT_U64"    },
    { "uint32_t",   "ALLOT_U32"    },
    { "uint16_t",   "ALLOT_U16"    },
    { "uint8_t",    "ALLOT_U8"     },
    { "int64_t",    "ALLOT_I64"    },
    { "int32_t",    "ALLOT_I32"    },
    { "int16_t",    "ALLOT_I16"    },
    { "int8_t",     "ALLOT_I8"     },
    { "bool",       "ALLOT_BOOL"   },
    { NULL, NULL }
};

static char*
S_allot_params_arg(CFCType *type, const char *label, int required) {
    const char *type_c_string = CFCType_to_c(type);
    unsigned label_len = (unsigned)strlen(label);
    const char *req_string = required ? "true" : "false";

    if (CFCType_is_object(type)) {
        const char *struct_sym = CFCType_get_specifier(type);
        const char *class_var  = CFCType_get_class_var(type);

        // Share buffers rather than copy between Perl scalars and Clownfish
        // string types.
        int use_sv_buffer = false;
        if (strcmp(struct_sym, "cfish_String") == 0
            || strcmp(struct_sym, "cfish_Obj") == 0
           ) {
            use_sv_buffer = true;
        }
        const char *allocation = use_sv_buffer
                                 ? "alloca(cfish_SStr_size())"
                                 : "NULL";
        const char pattern[] = "ALLOT_OBJ(&arg_%s, \"%s\", %u, %s, %s, %s)";
        char *arg = CFCUtil_sprintf(pattern, label, label, label_len,
                                    req_string, class_var, allocation);
        return arg;
    }
    else if (CFCType_is_primitive(type)) {
        for (int i = 0; prim_type_to_allot_macro[i].prim_type != NULL; i++) {
            const char *prim_type = prim_type_to_allot_macro[i].prim_type;
            if (strcmp(prim_type, type_c_string) == 0) {
                const char *allot = prim_type_to_allot_macro[i].allot_macro;
                char pattern[] = "%s(&arg_%s, \"%s\", %u, %s)";
                char *arg = CFCUtil_sprintf(pattern, allot, label, label,
                                            label_len, req_string);
                return arg;
            }
        }
    }

    CFCUtil_die("Missing typemap for %s", type_c_string);
    return NULL; // unreachable
}

char*
CFCPerlSub_arg_declarations(CFCPerlSub *self) {
    CFCParamList *param_list = self->param_list;
    CFCVariable **arg_vars   = CFCParamList_get_variables(param_list);
    size_t        num_vars   = CFCParamList_num_vars(param_list);
    char         *decls      = CFCUtil_strdup("");

    // Declare variables.
    for (size_t i = 1; i < num_vars; i++) {
        CFCVariable *arg_var  = arg_vars[i];
        CFCType     *type     = CFCVariable_get_type(arg_var);
        const char  *type_str = CFCType_to_c(type);
        const char  *var_name = CFCVariable_micro_sym(arg_var);
        decls = CFCUtil_cat(decls, "    ", type_str, " arg_", var_name,
                            ";\n", NULL);
    }

    return decls;
}

char*
CFCPerlSub_arg_name_list(CFCPerlSub *self) {
    CFCParamList  *param_list = self->param_list;
    CFCVariable  **arg_vars   = CFCParamList_get_variables(param_list);
    size_t        num_vars   = CFCParamList_num_vars(param_list);
    char          *name_list  = CFCUtil_strdup("arg_self");

    for (int i = 1; i < num_vars; i++) {
        CFCVariable *arg_var  = arg_vars[i];
        const char  *var_name = CFCVariable_micro_sym(arg_vars[i]);
        name_list = CFCUtil_cat(name_list, ", arg_", var_name, NULL);
    }

    return name_list;
}

char*
CFCPerlSub_build_allot_params(CFCPerlSub *self) {
    CFCParamList *param_list = self->param_list;
    CFCVariable **arg_vars   = CFCParamList_get_variables(param_list);
    const char  **arg_inits  = CFCParamList_get_initial_values(param_list);
    size_t        num_vars   = CFCParamList_num_vars(param_list);
    char *allot_params = CFCUtil_strdup("");

    // Declare variables and assign default values.
    for (size_t i = 1; i < num_vars; i++) {
        CFCVariable *arg_var  = arg_vars[i];
        const char  *val      = arg_inits[i];
        const char  *var_name = CFCVariable_micro_sym(arg_var);
        if (val == NULL) {
            CFCType *arg_type = CFCVariable_get_type(arg_var);
            val = CFCType_is_object(arg_type)
                  ? "NULL"
                  : "0";
        }
        allot_params = CFCUtil_cat(allot_params, "arg_", var_name, " = ", val,
                                   ";\n    ", NULL);
    }

    // Iterate over args in param list.
    allot_params
        = CFCUtil_cat(allot_params,
                      "args_ok = XSBind_allot_params(\n"
                      "        &(ST(0)), 1, items,\n", NULL);
    for (size_t i = 1; i < num_vars; i++) {
        CFCVariable *var = arg_vars[i];
        const char  *val = arg_inits[i];
        int required = val ? 0 : 1;
        const char *name = CFCVariable_micro_sym(var);
        CFCType *type = CFCVariable_get_type(var);
        char *arg = S_allot_params_arg(type, name, required);
        allot_params
            = CFCUtil_cat(allot_params, "        ", arg, ",\n", NULL);
        FREEMEM(arg);
    }
    allot_params
        = CFCUtil_cat(allot_params, "        NULL);\n",
                      "    if (!args_ok) {\n"
                      "        CFISH_RETHROW(CFISH_INCREF(cfish_Err_get_error()));\n"
                      "    }", NULL);

    return allot_params;
}

CFCParamList*
CFCPerlSub_get_param_list(CFCPerlSub *self) {
    return self->param_list;
}

const char*
CFCPerlSub_get_class_name(CFCPerlSub *self) {
    return self->class_name;
}

const char*
CFCPerlSub_get_alias(CFCPerlSub *self) {
    return self->alias;
}

int
CFCPerlSub_use_labeled_params(CFCPerlSub *self) {
    return self->use_labeled_params;
}

const char*
CFCPerlSub_perl_name(CFCPerlSub *self) {
    return self->perl_name;
}

const char*
CFCPerlSub_c_name(CFCPerlSub *self) {
    return self->c_name;
}

const char*
CFCPerlSub_c_name_list(CFCPerlSub *self) {
    return CFCParamList_name_list(self->param_list);
}

#endif
