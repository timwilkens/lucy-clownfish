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

#include <cmark.h>

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
#include "CFCUri.h"

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

static char*
S_nodes_to_pod(CFCClass *klass, cmark_node *node);

static char*
S_pod_escape(const char *content);

static char*
S_convert_link(CFCClass *klass, cmark_node *link);

static char*
S_pod_link(const char *text, const char *name);

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
            meth_pod = CFCUtil_sprintf("%s\n", meth_spec.pod);
        }
        else {
            meth_pod
                = CFCPerlPod_gen_subroutine_pod(self, (CFCFunction*)method,
                                                meth_spec.alias, klass,
                                                meth_spec.sample, class_name,
                                                false);
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
            pod = CFCUtil_cat(pod, slot.pod, "\n", NULL);
        }
        else {
            CFCFunction *init_func = CFCClass_function(klass, slot.func);
            char *sub_pod
                = CFCPerlPod_gen_subroutine_pod(self, init_func, slot.alias, klass,
                                                slot.sample, class_name, true);
            pod = CFCUtil_cat(pod, sub_pod, NULL);
            FREEMEM(sub_pod);
        }
    }
    return pod;
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
        char *perlified = CFCPerlPod_md_to_pod(self, klass, long_doc);
        pod = CFCUtil_cat(pod, perlified, NULL);
        FREEMEM(perlified);
    }

    // Add params in a list.
    const char**param_names = CFCDocuComment_get_param_names(docucomment);
    const char**param_docs  = CFCDocuComment_get_param_docs(docucomment);
    if (param_names[0]) {
        pod = CFCUtil_cat(pod, "=over\n\n", NULL);
        for (size_t i = 0; param_names[i] != NULL; i++) {
            char *perlified = CFCPerlPod_md_to_pod(self, klass, param_docs[i]);
            pod = CFCUtil_cat(pod, "=item *\n\nB<", param_names[i], "> - ",
                              perlified, NULL);
            FREEMEM(perlified);
        }
        pod = CFCUtil_cat(pod, "=back\n\n", NULL);
    }

    // Add return value description, if any.
    const char *retval_doc = CFCDocuComment_get_retval(docucomment);
    if (retval_doc && strlen(retval_doc)) {
        char *perlified = CFCPerlPod_md_to_pod(self, klass, retval_doc);
        pod = CFCUtil_cat(pod, "Returns: ", perlified, NULL);
        FREEMEM(perlified);
    }

    return pod;
}

char*
CFCPerlPod_md_to_pod(CFCPerlPod *self, CFCClass *klass, const char *md) {
    (void)self; // unused

    cmark_node *doc = cmark_parse_document(md, strlen(md));
    char *pod = S_nodes_to_pod(klass, doc);
    cmark_node_free(doc);

    return pod;
}

static char*
S_nodes_to_pod(CFCClass *klass, cmark_node *node) {
    char *result = CFCUtil_strdup("");
    if (node == NULL) {
        return result;
    }

    cmark_iter *iter = cmark_iter_new(node);
    cmark_event_type ev_type;

    while (CMARK_EVENT_DONE != (ev_type = cmark_iter_next(iter))) {
        cmark_node *node = cmark_iter_get_node(iter);
        cmark_node_type type = cmark_node_get_type(node);

        switch (type) {
            case CMARK_NODE_DOCUMENT:
                break;

            case CMARK_NODE_PARAGRAPH:
                if (ev_type == CMARK_EVENT_EXIT) {
                    result = CFCUtil_cat(result, "\n\n", NULL);
                }
                break;

            case CMARK_NODE_BLOCK_QUOTE:
            case CMARK_NODE_LIST:
                if (ev_type == CMARK_EVENT_ENTER) {
                    result = CFCUtil_cat(result, "=over\n\n", NULL);
                }
                else {
                    result = CFCUtil_cat(result, "=back\n\n", NULL);
                }
                break;

            case CMARK_NODE_ITEM:
                // TODO: Ordered lists.
                if (ev_type == CMARK_EVENT_ENTER) {
                    result = CFCUtil_cat(result, "=item *\n\n", NULL);
                }
                break;

            case CMARK_NODE_HEADER:
                if (ev_type == CMARK_EVENT_ENTER) {
                    int header_level = cmark_node_get_header_level(node);
                    char *header = CFCUtil_sprintf("=head%d ",
                                                   header_level + 2);
                    result = CFCUtil_cat(result, header, NULL);
                    FREEMEM(header);
                }
                else {
                    result = CFCUtil_cat(result, "\n\n", NULL);
                }
                break;

            case CMARK_NODE_CODE_BLOCK: {
                const char *content = cmark_node_get_literal(node);
                char *escaped = S_pod_escape(content);
                // Chomp trailing newline.
                size_t len = strlen(escaped);
                if (len > 0 && escaped[len-1] == '\n') {
                    escaped[len-1] = '\0';
                }
                char *indented
                    = CFCUtil_global_replace(escaped, "\n", "\n    ");
                result = CFCUtil_cat(result, "    ", indented, "\n\n", NULL);
                FREEMEM(indented);
                FREEMEM(escaped);
                break;
            }

            case CMARK_NODE_HTML: {
                const char *html = cmark_node_get_literal(node);
                result = CFCUtil_cat(result, "=begin html\n\n", html,
                                     "\n=end\n\n", NULL);
                break;
            }

            case CMARK_NODE_HRULE:
                break;

            case CMARK_NODE_TEXT: {
                const char *content = cmark_node_get_literal(node);
                char *escaped = S_pod_escape(content);
                result = CFCUtil_cat(result, escaped, NULL);
                FREEMEM(escaped);
                break;
            }

            case CMARK_NODE_LINEBREAK:
                // POD doesn't support line breaks. Start a new paragraph.
                result = CFCUtil_cat(result, "\n\n", NULL);
                break;

            case CMARK_NODE_SOFTBREAK:
                result = CFCUtil_cat(result, "\n", NULL);
                break;

            case CMARK_NODE_CODE: {
                const char *content = cmark_node_get_literal(node);
                char *escaped = S_pod_escape(content);
                result = CFCUtil_cat(result, "C<", escaped, ">", NULL);
                FREEMEM(escaped);
                break;
            }

            case CMARK_NODE_INLINE_HTML: {
                const char *html = cmark_node_get_literal(node);
                CFCUtil_warn("Inline HTML not supported in POD: %s", html);
                break;
            }

            case CMARK_NODE_LINK:
                if (ev_type == CMARK_EVENT_ENTER) {
                    char *pod = S_convert_link(klass, node);
                    result = CFCUtil_cat(result, pod, NULL);
                    FREEMEM(pod);
                    cmark_iter_reset(iter, node, CMARK_EVENT_EXIT);
                }
                break;

            case CMARK_NODE_IMAGE:
                CFCUtil_warn("Images not supported in POD");
                break;

            case CMARK_NODE_STRONG:
                if (ev_type == CMARK_EVENT_ENTER) {
                    result = CFCUtil_cat(result, "B<", NULL);
                }
                else {
                    result = CFCUtil_cat(result, ">", NULL);
                }
                break;

            case CMARK_NODE_EMPH:
                if (ev_type == CMARK_EVENT_ENTER) {
                    result = CFCUtil_cat(result, "I<", NULL);
                }
                else {
                    result = CFCUtil_cat(result, ">", NULL);
                }
                break;

            default:
                CFCUtil_die("Invalid cmark node type: %d", type);
                break;
        }
    }

    cmark_iter_free(iter);
    return result;
}

static char*
S_pod_escape(const char *content) {
    size_t  len        = strlen(content);
    size_t  result_len = 0;
    size_t  result_cap = len + 256;
    char   *result     = (char*)MALLOCATE(result_cap + 1);

    for (size_t i = 0; i < len; i++) {
        const char *subst      = content + i;
        size_t      subst_size = 1;

        switch (content[i]) {
            case '<':
                // Escape "less than".
                subst      = "E<lt>";
                subst_size = 5;
                break;
            case '>':
                // Escape "greater than".
                subst      = "E<gt>";
                subst_size = 5;
                break;
            case '|':
                // Escape vertical bar.
                subst      = "E<verbar>";
                subst_size = 9;
                break;
            case '=':
                // Escape equal sign at start of line.
                if (i == 0 || content[i-1] == '\n') {
                    subst      = "E<61>";
                    subst_size = 5;
                }
                break;
            default:
                break;
        }

        if (result_len + subst_size > result_cap) {
            result_cap += 256;
            result = (char*)REALLOCATE(result, result_cap + 1);
        }

        memcpy(result + result_len, subst, subst_size);
        result_len += subst_size;
    }

    result[result_len] = '\0';

    return result;
}

static char*
S_convert_link(CFCClass *klass, cmark_node *link) {
    cmark_node *child = cmark_node_first_child(link);
    const char *uri   = cmark_node_get_url(link);
    char       *text  = S_nodes_to_pod(klass, child);
    char       *retval;

    if (!CFCUri_is_clownfish_uri(uri)) {
        retval = S_pod_link(text, uri);
        FREEMEM(text);
        return retval;
    }

    char   *new_uri  = NULL;
    char   *new_text = NULL;
    CFCUri *uri_obj  = CFCUri_new(uri, klass);
    int     type     = CFCUri_get_type(uri_obj);

    switch (type) {
        case CFC_URI_NULL:
            // Change all instances of NULL to 'undef'
            new_text = CFCUtil_strdup("undef");
            break;

        case CFC_URI_CLASS: {
            const char *full_struct_sym = CFCUri_full_struct_sym(uri_obj);
            CFCClass *uri_class
                = CFCClass_fetch_by_struct_sym(full_struct_sym);

            if (!uri_class) {
                CFCUtil_warn("URI class not found: %s", full_struct_sym);
            }
            else if (uri_class != klass) {
                const char *class_name = CFCClass_get_class_name(uri_class);
                new_uri = CFCUtil_strdup(class_name);
            }

            if (text[0] != '\0') {
                // Keep text.
                break;
            }

            if (strcmp(CFCUri_get_prefix(uri_obj),
                       CFCClass_get_prefix(klass)) == 0
            ) {
                // Same parcel.
                const char *struct_sym = CFCUri_get_struct_sym(uri_obj);
                new_text = CFCUtil_strdup(struct_sym);
            }
            else {
                // Other parcel.
                if (!uri_class) {
                    new_text = CFCUtil_strdup(full_struct_sym);
                }
                else {
                    const char *class_name
                        = CFCClass_get_class_name(uri_class);
                    new_text = CFCUtil_strdup(class_name);
                }
            }

            break;
        }

        case CFC_URI_FUNCTION:
        case CFC_URI_METHOD: {
            const char *full_struct_sym = CFCUri_full_struct_sym(uri_obj);
            const char *func_sym        = CFCUri_get_func_sym(uri_obj);

            // Convert "Err_get_error" to "Clownfish->error".
            if (strcmp(full_struct_sym, "cfish_Err") == 0
                && strcmp(func_sym, "get_error") == 0
            ) {
                new_text = CFCUtil_strdup("Clownfish->error");
                break;
            }

            CFCClass *uri_class
                = CFCClass_fetch_by_struct_sym(full_struct_sym);

            // TODO: Link to relevant POD section. This isn't easy because
            // the section headers for functions also contain a description
            // of the parameters.

            if (!uri_class) {
                CFCUtil_warn("URI class not found: %s", full_struct_sym);
            }
            else if (uri_class != klass) {
                const char *class_name = CFCClass_get_class_name(uri_class);
                new_uri = CFCUtil_strdup(class_name);
            }

            new_text = CFCUtil_sprintf("%s()", func_sym);
            for (size_t i = 0; new_text[i] != '\0'; ++i) {
                new_text[i] = tolower(new_text[i]);
            }

            break;
        }
    }

    if (new_text) {
        FREEMEM(text);
        text = new_text;
    }

    if (new_uri) {
        retval = S_pod_link(text, new_uri);
        FREEMEM(new_uri);
        FREEMEM(text);
    }
    else {
        retval = text;
    }

    CFCBase_decref((CFCBase*)uri_obj);

    return retval;
}

static char*
S_pod_link(const char *text, const char *name) {
    if (!text || text[0] == '\0' || strcmp(text, name) == 0) {
        return CFCUtil_sprintf("L<%s>", name);
    }
    else {
        return CFCUtil_sprintf("L<%s|%s>", text, name);
    }
}

