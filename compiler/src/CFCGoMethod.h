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

#ifndef H_CFCGOMETHOD
#define H_CFCGOMETHOD

#ifdef __cplusplus
extern "C" {
#endif

/** Clownfish::CFC::Binding::Go::Method - Binding for an object method.
 *
 * 
 * This class isa Clownfish::CFC::Binding::Go::Subroutine -- see its
 * documentation for various code-generating routines.
 * 
 * Method bindings use labeled parameters if the C function takes more than one
 * argument (other than "self").  If there is only one argument, the binding
 * will be set up to accept a single positional argument.
 */
typedef struct CFCGoMethod CFCGoMethod;
struct CFCMethod;
#if 0

CFCGoMethod*
CFCGoMethod_new(struct CFCMethod *method);

/**
 * @param method A Clownfish::CFC::Model::Method.
 */
CFCGoMethod*
CFCGoMethod_init(CFCGoMethod *self, struct CFCMethod *method);

void
CFCGoMethod_destroy(CFCGoMethod *self);
#endif

/** Test whether bindings should be generated for a method.
  */
int
CFCGoMethod_can_be_bound(struct CFCMethod *method);

#if 0
/**
 * Create the Go name of the method.
 */
char*
CFCGoMethod_perl_name(struct CFCMethod *method);

/** Generate C code for the XSUB.
 */
char*
CFCGoMethod_xsub_def(CFCGoMethod *self);

#endif
/** Return C code implementing a callback to Go for this method.  This code
 * is run when a Go subclass has overridden a method in a Clownfish base
 * class.
 */
char*
CFCGoMethod_callback_def(struct CFCMethod *method);

#ifdef __cplusplus
}
#endif

#endif /* H_CFCGOMETHOD */

