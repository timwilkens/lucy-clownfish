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

/** Clownfish::CFC::Model::Class - An object representing a single class
 * definition.
 *
 * Clownfish::CFC::Model::Class objects are stored as quasi-singletons, one
 * for each unique parcel/class_name combination.
 */

#ifndef H_CFCCLASS
#define H_CFCCLASS

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

typedef struct CFCClass CFCClass;
struct CFCParcel;
struct CFCDocuComment;
struct CFCFunction;
struct CFCMethod;
struct CFCVariable;
struct CFCFileSpec;

/** Create and register a quasi-singleton.  May only be called once for each
 * unique parcel/class_name combination.
 *
 * @param parcel See Clownfish::CFC::Model::Symbol.
 * @param exposure See Clownfish::CFC::Model::Symbol.
 * @param class_name See Clownfish::CFC::Model::Symbol.
 * @param cnick See Clownfish::CFC::Model::Symbol.
 * @param micro_sym Defaults to "class".
 * @param docucomment An optional Clownfish::CFC::Model::DocuComment attached
 * to this class.
 * @param file_spec - Clownfish::CFC::Model::FileSpec of the file in which
 * this class was declared
 * @param docucomment A Clownfish::CFC::Model::DocuComment describing this
 * Class.
 * @param is_inert Should be true if the class is inert, i.e. cannot be
 * instantiated.
 * @param is_final Should be true if the class is final.
 */
CFCClass*
CFCClass_create(struct CFCParcel *parcel, const char *exposure,
                const char *class_name, const char *cnick,
                const char *micro_sym, struct CFCDocuComment *docucomment,
                struct CFCFileSpec *file_spec, const char *parent_class_name,
                int is_final, int is_inert);

CFCClass*
CFCClass_do_create(CFCClass *self, struct CFCParcel *parcel,
                   const char *exposure, const char *class_name,
                   const char *cnick, const char *micro_sym,
                   struct CFCDocuComment *docucomment,
                   struct CFCFileSpec *file_spec, const char *parent_class_name,
                   int is_final, int is_inert);

void
CFCClass_destroy(CFCClass *self);

/** Retrieve a Class, if one has already been created.
 *
 * @param A Clownfish::CFC::Model::Parcel.
 * @param class_name The name of the Class.
 */
CFCClass*
CFCClass_fetch_singleton(struct CFCParcel *parcel, const char *class_name);

/** Empty out the registry, decrementing the refcount of all Class singleton
 * objects.
 */
void
CFCClass_clear_registry(void);

/** Add a child class.
 */
void
CFCClass_add_child(CFCClass *self, CFCClass *child);

/** Add a Function to the class.  Valid only before CFCClass_grow_tree() is
 * called.
 */
void
CFCClass_add_function(CFCClass *self, struct CFCFunction *func);

/** Add a Method to the class.  Valid only before CFCClass_grow_tree() is
 * called.
 */
void
CFCClass_add_method(CFCClass *self, struct CFCMethod *method);

/** Add a member variable to the class.  Valid only before
 * CFCClass_grow_tree() is called.
 */
void
CFCClass_add_member_var(CFCClass *self, struct CFCVariable *var);

/** Add an inert (class) variable to the class.  Valid only before
 * CFCClass_grow_tree() is called.
 */
void
CFCClass_add_inert_var(CFCClass *self, struct CFCVariable *var);

/* Return the inert Function object for the supplied sym, if any.
 */
struct CFCFunction*
CFCClass_function(CFCClass *self, const char *sym);

/* Return the Method object for the supplied micro/macro sym, if any.
 */
struct CFCMethod*
CFCClass_method(CFCClass *self, const char *sym);

/** Return a Method object if the Method corresponding to the supplied sym is
 * implemented in this class.
 */
struct CFCMethod*
CFCClass_fresh_method(CFCClass *self, const char *sym);

/** Traverse all ancestors to find the first class which declared the method
 * and return it.  Cannot be called before grow_tree().
 */
struct CFCMethod*
CFCClass_find_novel_method(CFCClass *self, const char *sym);

/** Find the actual class of all object variables without prefix.
 */
void
CFCClass_resolve_types(CFCClass *self);

/** Bequeath all inherited methods and members to children.
 */
void
CFCClass_grow_tree(CFCClass *self);

/** Return this class and all its child classes as an array, where all
 * children appear after their parent nodes.
 */
CFCClass**
CFCClass_tree_to_ladder(CFCClass *self);

/** Return an array of all methods implemented in this class.
 */
struct CFCMethod**
CFCClass_fresh_methods(CFCClass *self);

/** Return an array of all member variables declared in this class.
 */
struct CFCVariable**
CFCClass_fresh_member_vars(CFCClass *self);

/** Return an array of all child classes.
 */
CFCClass**
CFCClass_children(CFCClass *self);

/** Return an array of all (inert) functions.
 */
struct CFCFunction**
CFCClass_functions(CFCClass *self);

/** Return an array of all methods.
 */
struct CFCMethod**
CFCClass_methods(CFCClass *self);

size_t
CFCClass_num_methods(CFCClass *self);

/** Return an array of all member variables.
 */
struct CFCVariable**
CFCClass_member_vars(CFCClass *self);

size_t
CFCClass_num_member_vars(CFCClass *self);

/** Return an array of all inert (shared, class) variables.
 */
struct CFCVariable**
CFCClass_inert_vars(CFCClass *self);

const char*
CFCClass_get_cnick(CFCClass *self);

/** Set the parent Class. (Not class name, Class.)
 */
void
CFCClass_set_parent(CFCClass *self, CFCClass *parent);

CFCClass*
CFCClass_get_parent(CFCClass *self);

const char*
CFCClass_get_path_part(CFCClass *self);

int
CFCClass_included(CFCClass *self);

const char*
CFCClass_get_parent_class_name(CFCClass *self);

int
CFCClass_final(CFCClass *self);

int
CFCClass_inert(CFCClass *self);

const char*
CFCClass_get_struct_sym(CFCClass *self);

/** Fully qualified struct symbol, including the parcel prefix.
 */
const char*
CFCClass_full_struct_sym(CFCClass *self);

/** IVARS struct name, not including parcel prefix.
 */
const char*
CFCClass_short_ivars_struct(CFCClass *self);

/** Fully qualified IVARS struct name including parcel prefix.
 */
const char*
CFCClass_full_ivars_struct(CFCClass *self);

/** Name of the function used to access IVARS, not including parcel prefix.
 */
const char*
CFCClass_short_ivars_func(CFCClass *self);

/** Fully qualified name of the function used to access IVARS including parcel
 * prefix.
 */
const char*
CFCClass_full_ivars_func(CFCClass *self);

/** Fully qualified name of the offset variable at which the IVARS may be
 * found, including parcel prefix.
 */
const char*
CFCClass_full_ivars_offset(CFCClass *self);

/** The short name of the global VTable object for this class.
 */
const char*
CFCClass_short_vtable_var(CFCClass *self);

/** Fully qualified vtable variable name, including the parcel prefix.
 */
const char*
CFCClass_full_vtable_var(CFCClass *self);

/** Access the symbol which unlocks the class struct definition and other
 * private information.
 */
const char*
CFCClass_privacy_symbol(CFCClass *self);

/** Return a relative path to a C header file, appropriately formatted for a
 * pound-include directive.
 */
const char*
CFCClass_include_h(CFCClass *self);

struct CFCDocuComment*
CFCClass_get_docucomment(CFCClass *self);

const char*
CFCClass_get_prefix(CFCClass *self);

const char*
CFCClass_get_Prefix(CFCClass *self);

const char*
CFCClass_get_PREFIX(CFCClass *self);

const char*
CFCClass_get_class_name(CFCClass *self);

struct CFCParcel*
CFCClass_get_parcel(CFCClass *self);

#ifdef __cplusplus
}
#endif

#endif /* H_CFCCLASS */

