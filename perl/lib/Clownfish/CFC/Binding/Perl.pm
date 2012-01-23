# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

use strict;
use warnings;

package Clownfish::CFC::Binding::Perl;

use Clownfish::CFC::Hierarchy;
use Carp;
use File::Spec::Functions qw( catfile );
use Fcntl;

use Clownfish::CFC::Parcel;
use Clownfish::CFC::Class;
use Clownfish::CFC::Function;
use Clownfish::CFC::Method;
use Clownfish::CFC::Variable;
use Clownfish::CFC::Util qw( verify_args a_isa_b write_if_changed );
use Clownfish::CFC::Binding::Perl::Class;
use Clownfish::CFC::Binding::Perl::Method;
use Clownfish::CFC::Binding::Perl::Constructor;

our %new_PARAMS = (
    parcel     => undef,
    hierarchy  => undef,
    lib_dir    => undef,
    boot_class => undef,
    header     => undef,
    footer     => undef,
);

sub new {
    my $either = shift;
    verify_args( \%new_PARAMS, @_ ) or confess $@;
    my $self = bless { %new_PARAMS, @_, }, ref($either) || $either;
    if ( !a_isa_b( $self->{parcel}, 'Clownfish::CFC::Parcel' ) ) {
        $self->{parcel}
            = Clownfish::CFC::Parcel->singleton( name => $self->{parcel} );
    }
    my $parcel = $self->{parcel};
    for ( keys %new_PARAMS ) {
        confess("$_ is mandatory") unless defined $self->{$_};
    }

    # Derive filenames.
    my $lib                = $self->{lib_dir};
    my $dest_dir           = $self->{hierarchy}->get_dest;
    my @file_components    = split( '::', $self->{boot_class} );
    my @xs_file_components = @file_components;
    $xs_file_components[-1] .= '.xs';
    $self->{xs_path} = catfile( $lib, @xs_file_components );

    $self->{pm_path} = catfile( $lib, @file_components, 'Autobinding.pm' );
    $self->{boot_h_file} = $parcel->get_prefix . "boot.h";
    $self->{boot_c_file} = $parcel->get_prefix . "boot.c";
    $self->{boot_h_path} = catfile( $dest_dir, $self->{boot_h_file} );
    $self->{boot_c_path} = catfile( $dest_dir, $self->{boot_c_file} );

    # Derive the name of the bootstrap function.
    $self->{boot_func}
        = $parcel->get_prefix . $self->{boot_class} . '_bootstrap';
    $self->{boot_func} =~ s/\W/_/g;

    return $self;
}

sub write_bindings {
    my $self           = shift;
    my $ordered        = $self->{hierarchy}->ordered_classes;
    my $registered     = Clownfish::CFC::Binding::Perl::Class->registered;
    my $hand_rolled_xs = "";
    my $generated_xs   = "";
    my $xs             = "";
    my @xsubs;

    # Build up a roster of all requested bindings.
    my %has_xs_code;
    for my $class (@$registered) {
        my $class_name = $class->get_class_name;
        my $class_binding
            = Clownfish::CFC::Binding::Perl::Class->singleton($class_name)
            or next;
        $has_xs_code{$class_name} = 1
            if $class_binding->get_xs_code;
    }

    # Pound-includes for generated headers.
    for my $class (@$ordered) {
        my $include_h = $class->include_h;
        $generated_xs .= qq|#include "$include_h"\n|;
    }
    $generated_xs .= "\n";

    # Constructors.
    for my $class (@$ordered) {
        my $class_name = $class->get_class_name;
        my $class_binding
            = Clownfish::CFC::Binding::Perl::Class->singleton($class_name);
        next unless $class_binding;
        my $bound = $class_binding->constructor_bindings;
        $generated_xs .= $_->xsub_def . "\n" for @$bound;
        push @xsubs, @$bound;
    }

    # Methods.
    for my $class (@$ordered) {
        my $class_name = $class->get_class_name;
        my $class_binding
            = Clownfish::CFC::Binding::Perl::Class->singleton($class_name);
        next unless $class_binding;
        my $bound = $class_binding->method_bindings;
        $generated_xs .= $_->xsub_def . "\n" for @$bound;
        push @xsubs, @$bound;
    }

    # Hand-rolled XS.
    for my $class_name ( keys %has_xs_code ) {
        my $class_binding
            = Clownfish::CFC::Binding::Perl::Class->singleton($class_name);
        $hand_rolled_xs .= $class_binding->get_xs_code . "\n";
    }
    %has_xs_code = ();

    # Verify that all binding specs were processed.
    my @leftover_xs = keys %has_xs_code;
    if (@leftover_xs) {
        confess(  "Hand-rolled XS spec'd for non-existant classes: "
                . "'@leftover_xs'" );
    }

    # Build up code for booting XSUBs at module load time.
    my @xs_init_lines;
    for my $xsub (@xsubs) {
        my $c_name    = $xsub->c_name;
        my $perl_name = $xsub->perl_name;
        push @xs_init_lines, qq|newXS("$perl_name", $c_name, file);|;
    }
    my $xs_init = join( "\n    ", @xs_init_lines );

    # Params hashes for arg checking of XSUBs that take labeled params.
    my @params_hash_defs = grep {defined} map { $_->params_hash_def } @xsubs;
    my $params_hash_defs = join( "\n", @params_hash_defs );

    # Write out if there have been any changes.
    my $xs_file_contents = $self->_xs_file_contents( $generated_xs, $xs_init,
        $hand_rolled_xs );
    my $pm_file_contents = $self->_pm_file_contents($params_hash_defs);
    write_if_changed( $self->{xs_path}, $xs_file_contents );
    write_if_changed( $self->{pm_path}, $pm_file_contents );
}

sub _xs_file_contents {
    my ( $self, $generated_xs, $xs_init, $hand_rolled_xs ) = @_;
    return <<END_STUFF;
/* DO NOT EDIT!!!! This is an auto-generated file. */

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

#include "XSBind.h"
#include "parcel.h"
#include "$self->{boot_h_file}"

#include "Lucy/Object/Host.h"
#include "Lucy/Util/Memory.h"
#include "Lucy/Util/StringHelper.h"

$generated_xs

MODULE = Lucy   PACKAGE = Lucy::Autobinding

void
init_autobindings()
PPCODE:
{
    char* file = __FILE__;
    CHY_UNUSED_VAR(cv);
    CHY_UNUSED_VAR(items); $xs_init
}

$hand_rolled_xs

END_STUFF
}

sub _pm_file_contents {
    my ( $self, $params_hash_defs ) = @_;
    return <<END_STUFF;
# DO NOT EDIT!!!! This is an auto-generated file.

# Licensed to the Apache Software Foundation (ASF) under one or more
# contributor license agreements.  See the NOTICE file distributed with
# this work for additional information regarding copyright ownership.
# The ASF licenses this file to You under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

use strict;
use warnings;

package Lucy::Autobinding;

init_autobindings();

$params_hash_defs

1;

END_STUFF
}

sub prepare_pod {
    my $self    = shift;
    my $lib_dir = $self->{lib_dir};
    my $ordered = $self->{hierarchy}->ordered_classes;
    my @files_written;
    my %has_pod;
    my %modified;

    my $registered = Clownfish::CFC::Binding::Perl::Class->registered;
    $has_pod{ $_->get_class_name } = 1
        for grep { $_->get_pod_spec } @$registered;

    for my $class (@$ordered) {
        my $class_name = $class->get_class_name;
        my $class_binding
            = Clownfish::CFC::Binding::Perl::Class->singleton($class_name)
            or next;
        next unless delete $has_pod{$class_name};
        my $pod = $class_binding->create_pod
            or confess("Failed to generate POD for $class_name");

        # Compare against existing file; rewrite if changed.
        my $pod_file_path
            = catfile( $lib_dir, split( '::', $class_name ) ) . ".pod";
        my $existing = "";
        if ( -e $pod_file_path ) {
            open( my $pod_fh, "<", $pod_file_path )
                or confess("Can't open '$pod_file_path': $!");
            $existing = do { local $/; <$pod_fh> };
        }
        if ( $pod ne $existing ) {
            $modified{$pod_file_path} = $pod;
        }
    }
    my @leftover = keys %has_pod;
    confess("Couldn't match pod to class for '@leftover'") if @leftover;

    return \%modified;
}

sub write_boot {
    my $self = shift;
    $self->_write_boot_h;
    $self->_write_boot_c;
}

sub _write_boot_h {
    my $self      = shift;
    my $hierarchy = $self->{hierarchy};
    my $filepath  = catfile( $hierarchy->get_dest, $self->{boot_h_file} );
    my $guard     = uc("$self->{boot_class}_BOOT");
    $guard =~ s/\W+/_/g;

    unlink $filepath;
    sysopen( my $fh, $filepath, O_CREAT | O_EXCL | O_WRONLY )
        or confess("Can't open '$filepath': $!");
    print $fh <<END_STUFF;
$self->{header}

#ifndef $guard
#define $guard 1

void
$self->{boot_func}();

#endif /* $guard */

$self->{footer}
END_STUFF
}

my %ks_compat = (
    'Lucy::Plan::Schema' =>
        [qw( KinoSearch::Plan::Schema KinoSearch::Schema )],
    'Lucy::Plan::FieldType' =>
        [qw( KinoSearch::Plan::FieldType KinoSearch::FieldType )],
    'Lucy::Plan::FullTextType' => [
        qw( KinoSearch::Plan::FullTextType KinoSearch::FieldType::FullTextType )
    ],
    'Lucy::Plan::StringType' => [
        qw( KinoSearch::Plan::StringType KinoSearch::FieldType::StringType )],
    'Lucy::Plan::BlobType' =>
        [qw( KinoSearch::Plan::BlobType KinoSearch::FieldType::BlobType )],
    'Lucy::Analysis::PolyAnalyzer' =>
        [qw( KinoSearch::Analysis::PolyAnalyzer )],
    'Lucy::Analysis::RegexTokenizer' =>
        [qw( KinoSearch::Analysis::Tokenizer )],
    'Lucy::Analysis::CaseFolder' => [
        qw( KinoSearch::Analysis::CaseFolder KinoSearch::Analysis::LCNormalizer )
    ],
    'Lucy::Analysis::SnowballStopFilter' =>
        [qw( KinoSearch::Analysis::Stopalizer )],
    'Lucy::Analysis::SnowballStemmer' =>
        [qw( KinoSearch::Analysis::Stemmer )],
);

sub _write_boot_c {
    my $self           = shift;
    my $hierarchy      = $self->{hierarchy};
    my $ordered        = $hierarchy->ordered_classes;
    my $num_classes    = scalar @$ordered;
    my $pound_includes = "";
    my $registrations  = "";
    my $isa_pushes     = "";

    for my $class (@$ordered) {
        my $include_h = $class->include_h;
        $pound_includes .= qq|#include "$include_h"\n|;
        next if $class->inert;

        # Ignore return value from VTable_add_to_registry, since it's OK if
        # multiple threads contend for adding these permanent VTables and some
        # fail.
        $registrations
            .= qq|    cfish_VTable_add_to_registry(|
            . $class->full_vtable_var
            . qq|);\n|;

        # Add aliases for selected KinoSearch classes which allow old indexes
        # to be read.
        my $class_name = $class->get_class_name;
        my $aliases    = $ks_compat{$class_name};
        if ($aliases) {
            my $vtable_var = $class->full_vtable_var;
            for my $alias (@$aliases) {
                my $len = length($alias);
                $registrations
                    .= qq|    Cfish_ZCB_Assign_Str(alias, "$alias", $len);\n|
                    . qq|    cfish_VTable_add_alias_to_registry($vtable_var,\n|
                    . qq|        (cfish_CharBuf*)alias);\n|;
            }
        }

        my $parent = $class->get_parent;
        next unless $parent;
        my $parent_class = $parent->get_class_name;
        $isa_pushes .= qq|    isa = get_av("$class_name\::ISA", 1);\n|;
        $isa_pushes .= qq|    av_push(isa, newSVpv("$parent_class", 0));\n|;
    }
    my $filepath = catfile( $hierarchy->get_dest, $self->{boot_c_file} );
    unlink $filepath;
    sysopen( my $fh, $filepath, O_CREAT | O_EXCL | O_WRONLY )
        or confess("Can't open '$filepath': $!");
    print $fh <<END_STUFF;
$self->{header}

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "$self->{boot_h_file}"
#include "parcel.h"
$pound_includes

void
$self->{boot_func}() {
    AV *isa;
    cfish_ZombieCharBuf *alias = CFISH_ZCB_WRAP_STR("", 0);
$registrations
$isa_pushes
}

$self->{footer}

END_STUFF
}

sub write_xs_typemap {
    my $self = shift;
    Clownfish::CFC::Binding::Perl::TypeMap->write_xs_typemap(
        hierarchy => $self->{hierarchy}, );
}

1;

__END__

__POD__

=head1 NAME

Clownfish::CFC::Binding::Perl - Perl bindings for a Clownfish::CFC::Hierarchy.

=head1 DESCRIPTION

Clownfish::CFC::Binding::Perl presents an interface for auto-generating XS and
Perl code to bind C code for a Clownfish class hierarchy to Perl.

In theory this module could be much more flexible and its API could be more
elegant.  There are many ways which you could walk the parsed parcels,
classes, methods, etc. in a Clownfish::CFC::Hierarchy and generate binding code.
However, our needs are very limited, so we are content with a "one size fits
one" solution.

In particular, this module assumes that the XS bindings for all classes in the
hierarchy should be assembled into a single shared object which belongs to the
primary, "boot" class.  There's no reason why it could not write one .xs file
per class, or one per parcel, instead.

The files written by this class are derived from the name of the boot class.
If it is "Crustacean", the following files will be generated.

    # Generated by write_bindings()
    $lib_dir/Crustacean.xs
    $lib_dir/Crustacean/Autobinding.pm

    # Generated by write_boot()
    $hierarchy_dest_dir/crust_boot.h
    $hierarchy_dest_dir/crust_boot.c

=head1 METHODS

=head2 new

    my $perl_binding = Clownfish::CFC::Binding::Perl->new(
        boot_class => 'Crustacean',                    # required
        parcel     => 'Crustacean',                    # required
        hierarchy  => $hierarchy,                      # required
        lib_dir    => 'lib',                           # required
        header     => "/* Autogenerated file */\n",    # required
        footer     => $copyfoot,                       # required
    );

=over

=item * B<boot_class> - The name of the main class, which will own the shared
object.

=item * B<parcel> - The L<Clownfish::CFC::Parcel> to which the C<boot_class>
belongs.

=item * B<hierarchy> - A Clownfish::CFC::Hierarchy.

=item * B<lib_dir> - location of the Perl lib directory to which files will be
written.

=item * B<header> - Text which will be prepended to generated C/XS files --
typically, an "autogenerated file" warning.

=item * B<footer> - Text to be appended to the end of generated C/XS files --
typically copyright information.

=back

=head2 write_bindings

    $perl_binding->write_bindings;

Generate the XS bindings (including "Autobind.pm) for all classes in the
hierarchy.

=head2 prepare_pod 

    my $filepaths_and_pod = $perl_binding->prepare_pod;
    while ( my ( $filepath, $pod ) = each %$filepaths_and_pod ) {
        add_to_cleanup($filepath);
        spew_file( $filepath, $pod );
    }

Auto-generate POD for all classes bindings which were spec'd with C<make_pod>
directives.  See whether a .pod file exists and is up to date.

Return a hash representing POD files that need to be modified; the keys are
filepaths, and the values are the POD file content.

The caller must take responsibility for actually writing out the POD files,
after adding the filepaths to cleanup records and so on.

=head2 write_boot

    $perl_binding->write_boot;

Write out "boot" files to the Hierarchy's C<dest_dir> which contain code for
bootstrapping Clownfish classes.

=cut
