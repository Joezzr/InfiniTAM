//  ================================================================
//  Created by Gregory Kramida on 1/2/20.
//  Copyright (c) 2020 Gregory Kramida
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at

//  http://www.apache.org/licenses/LICENSE-2.0

//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//  ================================================================
#pragma once

//stdlib
#include <string>
#include <unordered_map>
#include <regex>

//boost
#include <boost/algorithm/string/predicate.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/filesystem.hpp>
#include <boost/preprocessor/stringize.hpp>

// local
#include "SequenceLevel1Macros.h"
#include "PreprocessorNargs.h"
#include "MetacodingAuxiliaryUtilities.h"
#include "../FileIO/JSON_Utilities.h"
#include "../Logging/PrettyPrinters.h"
#include "../../../ORUtils/PlatformIndependence.h"


// region ==== OPTIONS_DESCRIPTION HELPER FUNCTIONS ============================
// namespace std {
// //definition required by boost::program_options to output default values of vector types
// template<typename T>
// std::ostream& operator<<(std::ostream& os, const std::vector<T>& vec) {
// 	for (auto item : vec) {
// 		os << item << " ";
// 	}
// 	return os;
// }
// }//namespace std

std::string find_snake_case_lowercase_acronym(const std::string& snake_case_identifier);
void generate_cli_argument_identifiers_snake_case(const boost::program_options::options_description& options_description,
                                                  const std::string& parent_long_identifier, const std::string& parent_short_identifier,
                                                  const std::string& field_name, std::string& long_identifier, std::string& short_identifier);

// endregion
// region ==== SERIALIZABLE STRUCT FUNCTION DEFINITIONS ==================

template<typename TSerializableStruct>
static boost::optional<TSerializableStruct>
ptree_to_optional_serializable_struct(const pt::ptree& tree, pt::ptree::key_type const& key,
                                      const std::string& origin) {
	auto subtree = tree.get_child_optional(key);
	if (subtree) {
		return boost::optional<TSerializableStruct>(TSerializableStruct::BuildFromPTree(subtree.get(), origin));
	} else {
		return boost::optional<TSerializableStruct>{};
	}
}

template<typename TSerializableStruct>
static TSerializableStruct
ExtractSerializableStructFromPtreeIfPresent(const pt::ptree& tree, pt::ptree::key_type const& key, const std::string& origin) {
	auto subtree = tree.get_child_optional(key);
	if (subtree) {
		return TSerializableStruct::BuildFromPTree(subtree.get(), origin);
	} else {
		return TSerializableStruct();
	}
}

// endregion
// region ================== SERIALIZABLE PATH FUNCTION DECLARATIONS ===================================================

std::string preprocess_path(const std::string& path, const std::string& origin);

std::string postprocess_path(const std::string& path, const std::string& origin);

boost::optional<std::string>
ptree_to_optional_path(const boost::property_tree::ptree& tree, const pt::ptree::key_type& key,
                       const std::string& origin);

// endregion
// region ================== SERIALIZABLE VECTOR FUNCTION DEFINITIONS ==================================================
template<typename TVector>
std::vector<typename TVector::value_type> serializable_vector_to_std_vector(TVector vector) {
	std::vector<typename TVector::value_type> std_vector;
	for (int i_element = 0; i_element < TVector::size(); i_element++) {
		std_vector.push_back(vector.values[i_element]);
	}
	return std_vector;
}

template<typename TVector>
TVector std_vector_to_serializable_vector(const std::vector<typename TVector::value_type>& std_vector) {
	TVector vector;
	if (std_vector.size() != TVector::size()) {
		DIEWITHEXCEPTION_REPORTLOCATION("Wrong number of elements in parsed vector.");
	}
	memcpy(vector.values, std_vector.data(), sizeof(typename TVector::value_type) * TVector::size());
	return vector;
}

std::string compile_sub_struct_parse_path(const std::string& current_parse_path, const std::string& sub_struct_instance_name);

template<typename TVector>
TVector variables_map_to_vector(const boost::program_options::variables_map& vm, const std::string& argument) {
	std::vector<typename TVector::value_type> std_vector = vm[argument].as<std::vector<typename TVector::value_type>>();
	return std_vector_to_serializable_vector<TVector>(std_vector);
}

template<typename TVector>
boost::optional<TVector> ptree_to_optional_serializable_vector(pt::ptree const& pt, pt::ptree::key_type const& key) {
	TVector vector;
	if (pt.count(key) == 0) {
		return boost::optional<TVector>{};
	}
	std::vector<typename TVector::value_type> std_vector;
	for (auto& item : pt.get_child(key)) {
		std_vector.push_back(item.second.get_value<typename TVector::value_type>());
	}
	return std_vector_to_serializable_vector<TVector>(std_vector);;
}

template<typename TVector>
boost::property_tree::ptree serializable_vector_to_ptree(TVector vector) {
	boost::property_tree::ptree tree;
	for (int i_element = 0; i_element < TVector::size(); i_element++) {
		boost::property_tree::ptree child;
		child.put("", vector.values[i_element]);
		tree.push_back(std::make_pair("", child));
	}
	return tree;
}

// endregion
// region ===== SERIALIZABLE STRUCT PER-FIELD MACROS ===================================================================

// *** used to declare fields & defaults ***
#define SERIALIZABLE_STRUCT_IMPL_FIELD_DECL(_, type, field_name, default_value, serialization_type, ...) \
    type field_name = default_value;

// *** used for a generic constructor that contains all fields ***
#define SERIALIZABLE_STRUCT_IMPL_TYPED_FIELD(_, type, field_name, ...) type field_name

#define SERIALIZABLE_STRUCT_IMPL_INIT_FIELD_ARG_PRIMITIVE(type, field_name) field_name ( field_name )
#define SERIALIZABLE_STRUCT_IMPL_INIT_FIELD_ARG_PATH(type, field_name) field_name ( std::move(field_name) )
#define SERIALIZABLE_STRUCT_IMPL_INIT_FIELD_ARG_ENUM(type, field_name) field_name ( field_name )
#define SERIALIZABLE_STRUCT_IMPL_INIT_FIELD_ARG_STRUCT(type, field_name) field_name ( std::move(field_name) )
#define SERIALIZABLE_STRUCT_IMPL_INIT_FIELD_ARG_VECTOR(type, field_name) field_name ( field_name )

#define SERIALIZABLE_STRUCT_IMPL_INIT_FIELD_ARG(_, type, field_name, default_value, serialization_type, ...) \
        ITM_METACODING_IMPL_CAT(SERIALIZABLE_STRUCT_IMPL_INIT_FIELD_ARG_, serialization_type)(type, field_name)

// *** variables_map --> value ***
#define SERIALIZABLE_STRUCT_IMPL_FIELD_VM_INIT_PRIMITIVE(type, field_name, default_value) \
    field_name(vm[compile_sub_struct_parse_path(parent_long_identifier, #field_name)].as<type>())
#define SERIALIZABLE_STRUCT_IMPL_FIELD_VM_INIT_PATH(type, field_name, default_value) \
    field_name(preprocess_path(vm[ compile_sub_struct_parse_path(parent_long_identifier, #field_name)  ].as<std::string>(), origin))
#define SERIALIZABLE_STRUCT_IMPL_FIELD_VM_INIT_ENUM(type, field_name, default_value) \
    field_name(string_to_enumerator< type >(vm[ compile_sub_struct_parse_path(parent_long_identifier, #field_name)  ].as<std::string>()))
#define SERIALIZABLE_STRUCT_IMPL_FIELD_VM_INIT_STRUCT(type, field_name, default_value) \
    field_name(vm, compile_sub_struct_parse_path(parent_long_identifier, #field_name))
#define SERIALIZABLE_STRUCT_IMPL_FIELD_VM_INIT_VECTOR(type, field_name, default_value) \
    field_name(variables_map_to_vector <type> (vm, compile_sub_struct_parse_path(parent_long_identifier, #field_name) ))

#define SERIALIZABLE_STRUCT_IMPL_FIELD_VM_INIT(struct_name, type, field_name, default_value, serialization_type, ...) \
    ITM_METACODING_IMPL_CAT(SERIALIZABLE_STRUCT_IMPL_FIELD_VM_INIT_, serialization_type)(type, field_name, default_value)

#define SERIALIZABLE_STRUCT_IMPL_FIELD_VM_UPDATE_PRIMITIVE(type, field_name) \
    if (!vm[long_identifier].defaulted())  this->field_name = vm[long_identifier].as<type>()
#define SERIALIZABLE_STRUCT_IMPL_FIELD_VM_UPDATE_PATH(type, field_name) \
    if (!vm[long_identifier].defaulted())  this->field_name = preprocess_path(vm[long_identifier].as<std::string>(), origin)
#define SERIALIZABLE_STRUCT_IMPL_FIELD_VM_UPDATE_ENUM(type, field_name) \
    if (!vm[long_identifier].defaulted())  this->field_name = string_to_enumerator< type >(vm[long_identifier].as<std::string>())
#define SERIALIZABLE_STRUCT_IMPL_FIELD_VM_UPDATE_STRUCT(type, field_name) \
    this->field_name.UpdateFromVariablesMap(vm, long_identifier )
#define SERIALIZABLE_STRUCT_IMPL_FIELD_VM_UPDATE_VECTOR(type, field_name) \
    if (!vm[long_identifier].defaulted())  this->field_name = variables_map_to_vector <type> (vm, long_identifier)

#define SERIALIZABLE_STRUCT_IMPL_FIELD_VM_UPDATE(struct_name, type, field_name, default_value, serialization_type, ...) \
	long_identifier = compile_sub_struct_parse_path(parent_long_identifier, #field_name); \
    ITM_METACODING_IMPL_CAT(SERIALIZABLE_STRUCT_IMPL_FIELD_VM_UPDATE_, serialization_type)(type, field_name)

// *** value --> options_description ***

#define SERIALIZABLE_STRUCT_IMPL_ADD_FIELD_TO_OPTIONS_DESCRIPTION_PRIMITIVE(type, field_name, default_value_in, description)\
    od.add_options()((field_long_identifier + "," + field_short_identifier).c_str(), \
    boost::program_options::value< type >()->default_value( default_value_in ), description);

#define SERIALIZABLE_STRUCT_IMPL_ADD_FIELD_TO_OPTIONS_DESCRIPTION_PATH(type, field_name, default_value_in, description)\
    od.add_options()((field_long_identifier + "," + field_short_identifier).c_str(), \
    boost::program_options::value< type >()->default_value( default_value_in ), description);

#define SERIALIZABLE_STRUCT_IMPL_ADD_FIELD_TO_OPTIONS_DESCRIPTION_ENUM(type, field_name, default_value_in, description)\
    od.add_options()((field_long_identifier + "," + field_short_identifier).c_str(), \
    boost::program_options::value< std::string >()->default_value( enumerator_to_string(default_value_in) ), \
    (std::string(description) + enumerator_bracketed_list< type>()).c_str());

#define SERIALIZABLE_STRUCT_IMPL_ADD_FIELD_TO_OPTIONS_DESCRIPTION_STRUCT(type, field_name, default_value, description)\
    type :: AddToOptionsDescription(od, field_long_identifier, field_short_identifier);

#define SERIALIZABLE_STRUCT_IMPL_ADD_FIELD_TO_OPTIONS_DESCRIPTION_VECTOR(type, field_name, default_value_in, description)\
    od.add_options()((field_long_identifier + "," + field_short_identifier).c_str(), \
    boost::program_options::value< std::vector< type::value_type> >()-> \
    multitoken()->default_value(serializable_vector_to_std_vector(default_value_in)), description);

#define SERIALIZABLE_STRUCT_IMPL_ADD_FIELD_TO_OPTIONS_DESCRIPTION(_, type, field_name, default_value, serialization_type, description) \
    generate_cli_argument_identifiers_snake_case(od, long_identifier, short_identifier, #field_name, field_long_identifier, field_short_identifier); \
    ITM_METACODING_IMPL_CAT(SERIALIZABLE_STRUCT_IMPL_ADD_FIELD_TO_OPTIONS_DESCRIPTION_, serialization_type)(type, field_name, default_value, description)

// *** ptree --> value ***

#define SERIALIZABLE_STRUCT_IMPL_FIELD_OPTIONAL_FROM_TREE_PRIMITIVE(type, field_name, default_value) \
    boost::optional< type > field_name = tree.get_optional< type > ( #field_name );
#define SERIALIZABLE_STRUCT_IMPL_FIELD_OPTIONAL_FROM_TREE_PATH(type, field_name, default_value) \
    boost::optional< type > field_name = ptree_to_optional_path( tree, #field_name, origin );
#define SERIALIZABLE_STRUCT_IMPL_FIELD_OPTIONAL_FROM_TREE_ENUM(type, field_name, default_value) \
    boost::optional< type > field_name = ptree_to_optional_enumerator< type >( tree, #field_name );
#define SERIALIZABLE_STRUCT_IMPL_FIELD_OPTIONAL_FROM_TREE_STRUCT(type, field_name, default_value) \
    boost::optional< type > field_name = ptree_to_optional_serializable_struct< type >( tree, #field_name, origin );
#define SERIALIZABLE_STRUCT_IMPL_FIELD_OPTIONAL_FROM_TREE_VECTOR(type, field_name, default_value) \
    boost::optional< type > field_name = ptree_to_optional_serializable_vector< type >(tree, #field_name);

#define SERIALIZABLE_STRUCT_IMPL_FIELD_OPTIONAL_FROM_TREE(_, type, field_name, default_value, serialization_type, ...) \
    ITM_METACODING_IMPL_CAT(SERIALIZABLE_STRUCT_IMPL_FIELD_OPTIONAL_FROM_TREE_, serialization_type)(type, field_name, default_value)

#define SERIALIZABLE_STRUCT_IMPL_FIELD_FROM_OPTIONAL(_, type, field_name, ...) \
    field_name ? field_name.get() : default_instance. field_name

// *** value --> ptree ***

#define SERIALIZABLE_STRUCT_IMPL_ADD_FIELD_TO_TREE_PRIMITIVE(type, field_name) \
    tree.add( #field_name , field_name );
#define SERIALIZABLE_STRUCT_IMPL_ADD_FIELD_TO_TREE_PATH(type, field_name) \
    tree.add( #field_name , postprocess_path( field_name, _origin ));
#define SERIALIZABLE_STRUCT_IMPL_ADD_FIELD_TO_TREE_ENUM(type, field_name) \
    tree.add( #field_name , enumerator_to_string( field_name ));
#define SERIALIZABLE_STRUCT_IMPL_ADD_FIELD_TO_TREE_STRUCT(type, field_name) \
    tree.add_child( #field_name , field_name .ToPTree(origin));
#define SERIALIZABLE_STRUCT_IMPL_ADD_FIELD_TO_TREE_VECTOR(type, field_name) \
    tree.add_child( #field_name , serializable_vector_to_ptree ( field_name ));

#define SERIALIZABLE_STRUCT_IMPL_ADD_FIELD_TO_TREE(_, type, field_name, default_value, serialization_type, ...) \
    ITM_METACODING_IMPL_CAT(SERIALIZABLE_STRUCT_IMPL_ADD_FIELD_TO_TREE_, serialization_type)(type, field_name)

// *** compare fields ***
#define SERIALIZABLE_STRUCT_IMPL_FIELD_COMPARISON(_, type, field_name, ...) \
    instance1. field_name == instance2. field_name


// endregion
// region ==== SERIALIZABLE STRUCT TOP-LEVEL MACROS =======================
// region ******************************************************** DECLARATION-ONLY ******************************************************************
#define SERIALIZABLE_STRUCT_DECL_IMPL(struct_name, ...) \
    SERIALIZABLE_STRUCT_DECL_IMPL_2(struct_name, ITM_METACODING_IMPL_NARG(__VA_ARGS__), __VA_ARGS__)

#define SERIALIZABLE_STRUCT_DECL_IMPL_2(struct_name, field_count, ...) \
    SERIALIZABLE_STRUCT_DECL_IMPL_3(struct_name, \
                             ITM_METACODING_IMPL_CAT(ITM_METACODING_IMPL_LOOP_, field_count), __VA_ARGS__)

#define SERIALIZABLE_STRUCT_DECL_IMPL_3(struct_name, loop, ...) \
    struct struct_name { \
        SERIALIZABLE_STRUCT_DECL_IMPL_BODY(struct_name, loop, __VA_ARGS__) \
    }

#define ORIGIN_AND_SOURCE_TREE() \
    std::string origin = ""; \
    boost::property_tree::ptree source_tree;

#define SERIALIZABLE_STRUCT_DECL_IMPL_MEMBER_VARS(loop, ...) \
    ITM_METACODING_IMPL_EXPAND(loop(SERIALIZABLE_STRUCT_IMPL_FIELD_DECL, _, \
                                           ITM_METACODING_IMPL_NOTHING, __VA_ARGS__))

#define SERIALIZABLE_STRUCT_DECL_IMPL_MEMBER_PATH_INDEPENDENT_FUNCS(struct_name, loop, ...) \
    struct_name (); \
    struct_name(ITM_METACODING_IMPL_EXPAND(loop(SERIALIZABLE_STRUCT_IMPL_TYPED_FIELD, _, \
                                                   ITM_METACODING_IMPL_COMMA, __VA_ARGS__)), \
        std::string origin = ""); \
    void SetFromPTree(const boost::property_tree::ptree& tree); \
    static struct_name BuildFromPTree(const boost::property_tree::ptree& tree, const std::string& origin = ""); \
    boost::property_tree::ptree ToPTree(const std::string& _origin = "") const; \
    friend bool operator==(const struct_name & instance1, const struct_name & instance2); \
    friend std::ostream& operator<<(std::ostream& out, const struct_name& instance);

#define SERIALIZABLE_STRUCT_DECL_IMPL_MEMBER_PATH_DEPENDENT_FUNCS(struct_name) \
	explicit struct_name (const boost::program_options::variables_map& vm, const std::string& parent_long_identifier = "", std::string origin = ""); \
	void UpdateFromVariablesMap(const boost::program_options::variables_map& vm, const std::string& parent_long_identifier = ""); \
	static void AddToOptionsDescription(boost::program_options::options_description& od, const std::string& long_identifier = "", const std::string& short_identifier = "");

#define SERIALIZABLE_STRUCT_DECL_IMPL_BODY(struct_name, loop, ...) \
        ORIGIN_AND_SOURCE_TREE() \
        SERIALIZABLE_STRUCT_DECL_IMPL_MEMBER_VARS(loop, __VA_ARGS__) \
        SERIALIZABLE_STRUCT_DECL_IMPL_MEMBER_PATH_INDEPENDENT_FUNCS(struct_name, loop, __VA_ARGS__) \
        SERIALIZABLE_STRUCT_DECL_IMPL_MEMBER_PATH_DEPENDENT_FUNCS(struct_name)

// endregion
// region ********************************************************* DEFINITION-ONLY ******************************************************************
#define SERIALIZABLE_STRUCT_DEFN_IMPL(outer_class, struct_name, ...) \
    SERIALIZABLE_STRUCT_DEFN_IMPL_2( outer_class, struct_name, ITM_METACODING_IMPL_NARG(__VA_ARGS__), __VA_ARGS__)

#define SERIALIZABLE_STRUCT_DEFN_HANDLE_QUALIFIER(qualifier) \
    ITM_METACODING_IMPL_IIF(ITM_METACODING_IMPL_ISEMPTY(qualifier)) \
    (qualifier, qualifier::)


#define SERIALIZABLE_STRUCT_DEFN_IMPL_2(outer_class, struct_name, field_count, ...) \
    SERIALIZABLE_STRUCT_DEFN_IMPL_3(SERIALIZABLE_STRUCT_DEFN_HANDLE_QUALIFIER(outer_class), \
                             SERIALIZABLE_STRUCT_DEFN_HANDLE_QUALIFIER(struct_name), struct_name, instance.source_tree=tree, , ,\
                             ITM_METACODING_IMPL_COMMA, origin(std::move(origin)), origin, , \
                             ITM_METACODING_IMPL_CAT(ITM_METACODING_IMPL_LOOP_, field_count), __VA_ARGS__)

#define SERIALIZABLE_STRUCT_DEFN_IMPL_3(outer_class, inner_qualifier, struct_name, source_tree_initializer, \
                                        friend_qualifier, static_qualifier, \
                                        ORIGIN_SEPARATOR, origin_initializer, origin_varname, default_string_arg, \
                                        loop, ...) \
	SERIALIZABLE_STRUCT_DEFN_IMPL_PATH_INDEPENDENT(outer_class, inner_qualifier, struct_name, source_tree_initializer, \
										friend_qualifier, static_qualifier, \
										ORIGIN_SEPARATOR, origin_initializer, origin_varname, default_string_arg, \
                                        loop, __VA_ARGS__) \
	SERIALIZABLE_STRUCT_DEFN_IMPL_PATH_DEPENDENT(outer_class, inner_qualifier, struct_name, source_tree_initializer, \
										friend_qualifier, static_qualifier, \
										ORIGIN_SEPARATOR, origin_initializer, origin_varname, default_string_arg, \
                                        loop, __VA_ARGS__)

#define SERIALIZABLE_STRUCT_DEFN_IMPL_PATH_DEPENDENT(outer_class, inner_qualifier, struct_name, source_tree_initializer, \
                                        friend_qualifier, static_qualifier, \
                                        ORIGIN_SEPARATOR, origin_initializer, origin_varname, default_string_arg, \
                                        loop, ...) \
	outer_class inner_qualifier struct_name(const boost::program_options::variables_map& vm, \
                                            const std::string& parent_long_identifier default_string_arg, \
                                            std::string origin default_string_arg) : \
            ITM_METACODING_IMPL_EXPAND(loop(SERIALIZABLE_STRUCT_IMPL_FIELD_VM_INIT, struct_name, \
                                               ITM_METACODING_IMPL_COMMA, __VA_ARGS__)) \
            ORIGIN_SEPARATOR() origin_initializer \
        {} \
	void outer_class inner_qualifier UpdateFromVariablesMap(const boost::program_options::variables_map& vm, \
                                                            const std::string& parent_long_identifier default_string_arg){ \
        std::string long_identifier; \
        ITM_METACODING_IMPL_EXPAND(loop(SERIALIZABLE_STRUCT_IMPL_FIELD_VM_UPDATE, struct_name, \
                                               ITM_METACODING_IMPL_SEMICOLON, __VA_ARGS__)); \
    } \
    static_qualifier void outer_class inner_qualifier AddToOptionsDescription( \
            boost::program_options::options_description& od, \
            const std::string& long_identifier default_string_arg, \
            const std::string& short_identifier default_string_arg) { \
        std::string field_long_identifier, field_short_identifier; \
        ITM_METACODING_IMPL_EXPAND(loop(SERIALIZABLE_STRUCT_IMPL_ADD_FIELD_TO_OPTIONS_DESCRIPTION, _, \
                                           ITM_METACODING_IMPL_NOTHING, __VA_ARGS__)) \
    }


#define SERIALIZABLE_STRUCT_DEFN_IMPL_PATH_INDEPENDENT(outer_class, inner_qualifier, struct_name, source_tree_initializer, \
                                        friend_qualifier, static_qualifier, \
                                        ORIGIN_SEPARATOR, origin_initializer, origin_varname, default_string_arg, \
                                        loop, ...) \
    outer_class inner_qualifier struct_name () = default; \
    outer_class inner_qualifier struct_name( \
        ITM_METACODING_IMPL_EXPAND(loop(SERIALIZABLE_STRUCT_IMPL_TYPED_FIELD, _, ITM_METACODING_IMPL_COMMA, \
                                           __VA_ARGS__)), \
        std::string origin default_string_arg): \
            ITM_METACODING_IMPL_EXPAND(loop(SERIALIZABLE_STRUCT_IMPL_INIT_FIELD_ARG, _, \
                                               ITM_METACODING_IMPL_COMMA, __VA_ARGS__)) \
            ORIGIN_SEPARATOR() origin_initializer \
            {} \
    void outer_class inner_qualifier SetFromPTree(const boost::property_tree::ptree& tree){ \
        struct_name temporary_instance = BuildFromPTree(tree);\
        *this = temporary_instance;\
    } \
    static_qualifier outer_class struct_name outer_class inner_qualifier BuildFromPTree(const boost::property_tree::ptree& tree, \
                                                                    const std::string& origin default_string_arg){ \
        struct_name default_instance; \
        ITM_METACODING_IMPL_EXPAND(loop(SERIALIZABLE_STRUCT_IMPL_FIELD_OPTIONAL_FROM_TREE, _, \
                                           ITM_METACODING_IMPL_NOTHING, __VA_ARGS__)) \
        struct_name instance = { \
            ITM_METACODING_IMPL_EXPAND(loop(SERIALIZABLE_STRUCT_IMPL_FIELD_FROM_OPTIONAL, _, \
                                               ITM_METACODING_IMPL_COMMA, __VA_ARGS__)) \
            ORIGIN_SEPARATOR() origin_varname \
        }; \
        source_tree_initializer; \
        return instance; \
    } \
    boost::property_tree::ptree outer_class inner_qualifier ToPTree(const std::string& _origin default_string_arg) const { \
        boost::property_tree::ptree tree; \
        ITM_METACODING_IMPL_EXPAND(loop(SERIALIZABLE_STRUCT_IMPL_ADD_FIELD_TO_TREE, _, \
                                           ITM_METACODING_IMPL_NOTHING, __VA_ARGS__)) \
        return tree; \
    } \
    friend_qualifier bool operator==(const outer_class struct_name & instance1, const outer_class struct_name & instance2){ \
        return \
        ITM_METACODING_IMPL_EXPAND(loop(SERIALIZABLE_STRUCT_IMPL_FIELD_COMPARISON, _, ITM_METACODING_IMPL_AND, \
                                           __VA_ARGS__)); \
    } \
    friend_qualifier std::ostream& operator<<(std::ostream& out, const outer_class struct_name& instance) { \
        boost::property_tree::ptree tree(instance.ToPTree()); \
        boost::property_tree::write_json_no_quotes(out, tree, true); \
        return out; \
    }

// endregion
// *** FULL STRUCT GENERATION ***
#define SERIALIZABLE_STRUCT_IMPL(struct_name, ...) \
    SERIALIZABLE_STRUCT_IMPL_2(struct_name, ITM_METACODING_IMPL_NARG(__VA_ARGS__), __VA_ARGS__)

#define SERIALIZABLE_STRUCT_IMPL_2(struct_name, field_count, ...) \
    SERIALIZABLE_STRUCT_IMPL_3(struct_name, \
                             ITM_METACODING_IMPL_CAT(ITM_METACODING_IMPL_LOOP_, field_count), __VA_ARGS__)

#define SERIALIZABLE_STRUCT_IMPL_3(struct_name, loop, ...) \
    struct struct_name { \
        ORIGIN_AND_SOURCE_TREE() \
        SERIALIZABLE_STRUCT_DECL_IMPL_MEMBER_VARS(loop, __VA_ARGS__) \
        SERIALIZABLE_STRUCT_DEFN_IMPL_3 ( , , struct_name, instance.source_tree=tree, friend, static, \
        ITM_METACODING_IMPL_COMMA, origin(std::move(origin)), origin, ="", \
        loop, __VA_ARGS__) \
    };

// endregion



