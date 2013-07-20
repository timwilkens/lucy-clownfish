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
#include <ctype.h>
#define CFC_NEED_BASE_STRUCT_DEF
#include "CFCBase.h"
#include "CFCPerlPod.h"
#include "CFCUtil.h"
#include "CFCClass.h"
#include "CFCMethod.h"
#include "CFCParcel.h"
#include "CFCParamList.h"
#include "CFCFunction.h"
#include "CFCDocuComment.h"

#ifndef true
  #define true 1
  #define false 0
#endif

typedef struct NamePod {
    char *alias;
    char *func;
    char *sample;
    char *pod;
} NamePod;

struct CFCPerlPod {
    CFCBase base;
    char    *synopsis;
    char    *description;
    NamePod *methods;
    size_t   num_methods;
    NamePod *constructors;
    size_t   num_constructors;
};

static const CFCMeta CFCPERLPOD_META = {
    "Clownfish::CFC::Binding::Perl::Pod",
    sizeof(CFCPerlPod),
    (CFCBase_destroy_t)CFCPerlPod_destroy
};

CFCPerlPod*
CFCPerlPod_new(void) {
    CFCPerlPod *self
        = (CFCPerlPod*)CFCBase_allocate(&CFCPERLPOD_META);
    return CFCPerlPod_init(self);
}

CFCPerlPod*
CFCPerlPod_init(CFCPerlPod *self) {
    self->synopsis         = CFCUtil_strdup("");
    self->description      = CFCUtil_strdup("");
    self->methods          = NULL;
    self->constructors     = NULL;
    self->num_methods      = 0;
    self->num_constructors = 0;
    return self;
}

void
CFCPerlPod_destroy(CFCPerlPod *self) {
    FREEMEM(self->synopsis);
    FREEMEM(self->description);
    for (size_t i = 0; i < self->num_methods; i++) {
        FREEMEM(self->methods[i].alias);
        FREEMEM(self->methods[i].pod);
        FREEMEM(self->methods[i].func);
        FREEMEM(self->methods[i].sample);
    }
    FREEMEM(self->methods);
    for (size_t i = 0; i < self->num_constructors; i++) {
        FREEMEM(self->constructors[i].alias);
        FREEMEM(self->constructors[i].pod);
        FREEMEM(self->constructors[i].func);
        FREEMEM(self->constructors[i].sample);
    }
    FREEMEM(self->constructors);
    CFCBase_destroy((CFCBase*)self);
}

void
CFCPerlPod_add_method(CFCPerlPod *self, const char *alias, const char *method,
                      const char *sample, const char *pod) {
    CFCUTIL_NULL_CHECK(alias);
    self->num_methods++;
    size_t size = self->num_methods * sizeof(NamePod);
    self->methods = (NamePod*)REALLOCATE(self->methods, size);
    NamePod *slot = &self->methods[self->num_methods - 1];
    slot->alias  = CFCUtil_strdup(alias);
    slot->func   = method ? CFCUtil_strdup(method) : NULL;
    slot->sample = CFCUtil_strdup(sample ? sample : "");
    slot->pod    = pod ? CFCUtil_strdup(pod) : NULL;
}

void
CFCPerlPod_add_constructor(CFCPerlPod *self, const char *alias,
                           const char *initializer, const char *sample,
                           const char *pod) {
    self->num_constructors++;
    size_t size = self->num_constructors * sizeof(NamePod);
    self->constructors = (NamePod*)REALLOCATE(self->constructors, size);
    NamePod *slot = &self->constructors[self->num_constructors - 1];
    slot->alias  = CFCUtil_strdup(alias ? alias : "new");
    slot->func   = CFCUtil_strdup(initializer ? initializer : "init");
    slot->sample = CFCUtil_strdup(sample ? sample : "");
    slot->pod    = pod ? CFCUtil_strdup(pod) : NULL;
}

void
CFCPerlPod_set_synopsis(CFCPerlPod *self, const char *synopsis) {
    FREEMEM(self->synopsis);
    self->synopsis = CFCUtil_strdup(synopsis);
}

const char*
CFCPerlPod_get_synopsis(CFCPerlPod *self) {
    return self->synopsis;
}

void
CFCPerlPod_set_description(CFCPerlPod *self, const char *description) {
    FREEMEM(self->description);
    self->description = CFCUtil_strdup(description);
}

const char*
CFCPerlPod_get_description(CFCPerlPod *self) {
    return self->description;
}

char*
CFCPerlPod_methods_pod(CFCPerlPod *self, CFCClass *klass) {
    const char *class_name = CFCClass_get_class_name(klass);
    char *abstract_pod = CFCUtil_strdup("");
    char *methods_pod  = CFCUtil_strdup("");
    for (size_t i = 0; i < self->num_methods; i++) {
        NamePod meth_spec = self->methods[i];
        CFCMethod *method = CFCClass_method(klass, meth_spec.func);
        if (!method) {
            method = CFCClass_method(klass, meth_spec.alias);
        }
        if (!method) {
            CFCUtil_die("Can't find method '%s' in class '%s'",
                        meth_spec.alias, CFCClass_get_class_name(klass));
        }
        char *meth_pod;
        if (meth_spec.pod) {
            meth_pod = CFCPerlPod_perlify_doc_text(self, meth_spec.pod);
        }
        else {
            char *raw
                = CFCPerlPod_gen_subroutine_pod(self, (CFCFunction*)method,
                                                meth_spec.alias, klass,
                                                meth_spec.sample, class_name,
                                                false);
            meth_pod = CFCPerlPod_perlify_doc_text(self, raw);
            FREEMEM(raw);
        }
        if (CFCMethod_abstract(method)) {
            abstract_pod = CFCUtil_cat(abstract_pod, meth_pod, NULL);
        }
        else {
            methods_pod = CFCUtil_cat(methods_pod, meth_pod, NULL);
        }
        FREEMEM(meth_pod);
    }

    char *pod = CFCUtil_strdup("");
    if (strlen(abstract_pod)) {
        pod = CFCUtil_cat(pod, "=head1 ABSTRACT METHODS\n\n", abstract_pod, NULL);
    }
    FREEMEM(abstract_pod);
    if (strlen(methods_pod)) {
        pod = CFCUtil_cat(pod, "=head1 METHODS\n\n", methods_pod, NULL);
    }
    FREEMEM(methods_pod);

    return pod;
}

char*
CFCPerlPod_constructors_pod(CFCPerlPod *self, CFCClass *klass) {
    if (!self->num_constructors) {
        return CFCUtil_strdup("");
    }
    const char *class_name = CFCClass_get_class_name(klass);
    char *pod = CFCUtil_strdup("=head1 CONSTRUCTORS\n\n");
    for (size_t i = 0; i < self->num_constructors; i++) {
        NamePod slot = self->constructors[i];
        if (slot.pod) {
            char *perlified = CFCPerlPod_perlify_doc_text(self, slot.pod);
            pod = CFCUtil_cat(pod, perlified, NULL);
            FREEMEM(perlified);
        }
        else {
            CFCFunction *init_func = CFCClass_function(klass, slot.func);
            if (!init_func) {
                CFCUtil_die("Can't find function '%s' in class '%s'",
                            slot.func, CFCClass_get_class_name(klass));
            }
            char *sub_pod
                = CFCPerlPod_gen_subroutine_pod(self, init_func, slot.alias, klass,
                                                slot.sample, class_name, true);
            char *perlified = CFCPerlPod_perlify_doc_text(self, sub_pod);
            pod = CFCUtil_cat(pod, perlified, NULL);
            FREEMEM(sub_pod);
            FREEMEM(perlified);
        }
    }
    return pod;
}

static char*
S_global_replace(const char *string, const char *match,
                 const char *replacement) {
    char *found = (char*)string;
    int   string_len      = (int)strlen(string);
    int   match_len       = (int)strlen(match);
    int   replacement_len = (int)strlen(replacement);
    int   len_diff        = replacement_len - match_len;

    // Allocate space.
    unsigned count = 0;
    while (NULL != (found = strstr(found, match))) {
        count++;
        found += match_len;
    }
    int size = string_len + count * len_diff + 1;
    char *modified = (char*)MALLOCATE(size);
    modified[size - 1] = 0; // NULL-terminate.

    // Iterate through all matches.
    found = (char*)string;
    char *target = modified;
    size_t last_end = 0;
    if (count) {
        while (NULL != (found = strstr(found, match))) {
            size_t pos = found - string;
            size_t unchanged_len = pos - last_end;
            found += match_len;
            memcpy(target, string + last_end, unchanged_len);
            target += unchanged_len;
            last_end = pos + match_len;
            memcpy(target, replacement, replacement_len);
            target += replacement_len;
        }
    }
    size_t remaining = string_len - last_end;
    memcpy(target, string + string_len - remaining, remaining);

    return modified;
}

char*
CFCPerlPod_gen_subroutine_pod(CFCPerlPod *self, CFCFunction *func,
                              const char *alias, CFCClass *klass,
                              const char *code_sample,
                              const char *class_name, int is_constructor) {
    // Only allow "public" subs to be exposed as part of the public API.
    if (!CFCFunction_public(func)) {
        CFCUtil_die("%s#%s is not public", class_name, alias);
    }

    CFCParamList *param_list = CFCFunction_get_param_list(func);
    int num_vars = (int)CFCParamList_num_vars(param_list);
    char *pod = CFCUtil_sprintf("=head2 %s", alias);

    // Get documentation, which may be inherited.
    CFCDocuComment *docucomment = CFCFunction_get_docucomment(func);
    if (!docucomment) {
        const char *micro_sym = CFCFunction_micro_sym(func);
        CFCClass *parent = klass;
        while (NULL != (parent = CFCClass_get_parent(parent))) {
            CFCFunction *parent_func
                = (CFCFunction*)CFCClass_method(parent, micro_sym);
            if (!parent_func) { break; }
            docucomment = CFCFunction_get_docucomment(parent_func);
            if (docucomment) { break; }
        }
    }
    if (!docucomment) {
        CFCUtil_die("No DocuComment for '%s' in '%s'", alias, class_name);
    }

    // Build string summarizing arguments to use in header.
    if (num_vars > 2 || (is_constructor && num_vars > 1)) {
        pod = CFCUtil_cat(pod, "( I<[labeled params]> )\n\n", NULL);
    }
    else if (num_vars == 2) {
        // Kill self param.
        const char *name_list = CFCParamList_name_list(param_list);
        const char *after_comma = strchr(name_list, ',') + 1;
        while (isspace(*after_comma)) { after_comma++; }
        pod = CFCUtil_cat(pod, "(", after_comma, ")\n\n", NULL);
    }
    else {
        // num_args == 1, leave off 'self'.
        pod = CFCUtil_cat(pod, "()\n\n", NULL);
    }

    // Add code sample.
    if (code_sample && strlen(code_sample)) {
        pod = CFCUtil_cat(pod, code_sample, "\n", NULL);
    }

    // Incorporate "description" text from DocuComment.
    const char *long_doc = CFCDocuComment_get_description(docucomment);
    if (long_doc && strlen(long_doc)) {
        char *perlified = CFCPerlPod_perlify_doc_text(self, long_doc);
        pod = CFCUtil_cat(pod, perlified, "\n\n", NULL);
        FREEMEM(perlified);
    }

    // Add params in a list.
    const char**param_names = CFCDocuComment_get_param_names(docucomment);
    const char**param_docs  = CFCDocuComment_get_param_docs(docucomment);
    if (param_names[0]) {
        pod = CFCUtil_cat(pod, "=over\n\n", NULL);
        for (size_t i = 0; param_names[i] != NULL; i++) {
            pod = CFCUtil_cat(pod, "=item *\n\nB<", param_names[i], "> - ",
                              param_docs[i], "\n\n", NULL);
        }
        pod = CFCUtil_cat(pod, "=back\n\n", NULL);
    }

    // Add return value description, if any.
    const char *retval_doc = CFCDocuComment_get_retval(docucomment);
    if (retval_doc && strlen(retval_doc)) {
        pod = CFCUtil_cat(pod, "Returns: ", retval_doc, "\n\n", NULL);
    }

    return pod;
}

char*
CFCPerlPod_perlify_doc_text(CFCPerlPod *self, const char *source) {
    (void)self; // unused

    // Change <code>foo</code> to C<< foo >>.
    char *copy = CFCUtil_strdup(source);
    char *orig = copy;
    copy = S_global_replace(orig, "<code>", "C<< ");
    FREEMEM(orig);
    orig = copy;
    copy = S_global_replace(orig, "</code>", " >>");
    FREEMEM(orig);

    // Lowercase all method names: Open_In() => open_in()
    for (size_t i = 0, max = strlen(copy); i < max; i++) {
        if (isupper(copy[i])) {
            size_t mark = i;
            for (; i < max; i++) {
                char c = copy[i];
                if (!(isalpha(c) || c == '_')) {
                    if (memcmp(copy + i, "()", 2) == 0) {
                        for (size_t j = mark; j < i; j++) {
                            copy[j] = tolower(copy[j]);
                        }
                        i += 2; // go past parens.
                    }
                    break;
                }
            }
        }
    }

    // Change all instances of NULL to 'undef'
    orig = copy;
    copy = S_global_replace(orig, "NULL", "undef");
    FREEMEM(orig);

    // Change "Err_error" to "Clownfish->error".
    orig = copy;
    copy = S_global_replace(orig, "Err_error", "Clownfish->error");
    FREEMEM(orig);

    return copy;
}

