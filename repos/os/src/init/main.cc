/*
 * \brief  Init process
 * \author Norman Feske
 * \date   2010-04-27
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <init/child.h>
#include <base/sleep.h>
#include <os/config.h>


namespace Init { bool config_verbose = false; }


/***************
 ** Utilities **
 ***************/

/**
 * Read priority-levels declaration from config
 */
inline long read_prio_levels_log2()
{
	using namespace Genode;

	long prio_levels = 0;
	try {
		config()->xml_node().attribute("prio_levels").value(&prio_levels); }
	catch (...) { }

	if (prio_levels && prio_levels != (1 << log2(prio_levels))) {
		printf("Warning: Priolevels is not power of two, priorities are disabled\n");
		prio_levels = 0;
	}
	return prio_levels ? log2(prio_levels) : 0;
}


/**
 * Read affinity-space parameters from config
 *
 * If no affinity space is declared, construct a space with a single element,
 * width and height being 1. If only one of both dimensions is specified, the
 * other dimension is set to 1.
 */
inline Genode::Affinity::Space read_affinity_space()
{
	using namespace Genode;
	try {
		Xml_node node = config()->xml_node().sub_node("affinity-space");
		return Affinity::Space(node.attribute_value<unsigned long>("width",  1),
		                       node.attribute_value<unsigned long>("height", 1));
	} catch (...) {
		return Affinity::Space(1, 1); }
}


/**
 * Read parent-provided services from config
 */
inline void determine_parent_services(Genode::Service_registry *services)
{
	using namespace Genode;

	if (Init::config_verbose)
		printf("parent provides\n");

	Xml_node node = config()->xml_node().sub_node("parent-provides").sub_node("service");
	for (; ; node = node.next("service")) {

		char service_name[Genode::Service::MAX_NAME_LEN];
		node.attribute("name").value(service_name, sizeof(service_name));

		Parent_service *s = new (env()->heap()) Parent_service(service_name);
		services->insert(s);
		if (Init::config_verbose)
			printf("  service \"%s\"\n", service_name);

		if (node.is_last("service")) break;
	}
}


/********************
 ** Child registry **
 ********************/

namespace Init { struct Alias; }


/**
 * Representation of an alias for a child
 */
struct Init::Alias : Genode::List<Alias>::Element
{
	typedef Genode::String<128> Name;
	typedef Genode::String<128> Child;

	Name  name;
	Child child;

	/**
	 * Exception types
	 */
	class Name_is_missing  { };
	class Child_is_missing { };

	/**
	 * Utility to read a string attribute from an XML node
	 *
	 * \param STR        string type
	 * \param EXC        exception type raised if attribute is not present
	 *
	 * \param node       XML node
	 * \param attr_name  name of attribute to read
	 */
	template <typename STR, typename EXC>
	static STR _read_string_attr(Genode::Xml_node node, char const *attr_name)
	{
		char buf[STR::size()];

		if (!node.has_attribute(attr_name))
			throw EXC();

		node.attribute(attr_name).value(buf, sizeof(buf));

		return STR(buf);
	}

	/**
	 * Constructor
	 *
	 * \throw Name_is_missing
	 * \throw Child_is_missing
	 */
	Alias(Genode::Xml_node alias)
	:
		name (_read_string_attr<Name, Name_is_missing> (alias, "name")),
		child(_read_string_attr<Name, Child_is_missing>(alias, "child"))
	{ }
};


namespace Init {

	typedef Genode::List<Genode::List_element<Child> > Child_list;

	struct Child_registry;
}


class Init::Child_registry : public Name_registry, Child_list
{
	private:

		List<Alias> _aliases;

	public:

		/**
		 * Exception type
		 */
		class Alias_name_is_not_unique { };

		/**
		 * Register child
		 */
		void insert(Child *child)
		{
			Child_list::insert(&child->_list_element);
		}

		/**
		 * Unregister child
		 */
		void remove(Child *child)
		{
			Child_list::remove(&child->_list_element);
		}

		/**
		 * Register alias
		 */
		void insert_alias(Alias *alias)
		{
			if (!is_unique(alias->name.string())) {
				PERR("Alias name %s is not unique", alias->name.string());
				throw Alias_name_is_not_unique();
			}
			_aliases.insert(alias);
		}

		/**
		 * Unregister alias
		 */
		void remove_alias(Alias *alias)
		{
			_aliases.remove(alias);
		}

		/**
		 * Start execution of all children
		 */
		void start()
		{
			Genode::List_element<Child> *curr = first();
			for (; curr; curr = curr->next())
				curr->object()->start();
		}

		/**
		 * Return any of the registered children, or 0 if no child exists
		 */
		Child *any()
		{
			return first() ? first()->object() : 0;
		}

		/**
		 * Return any of the registered aliases, or 0 if no alias exists
		 */
		Alias *any_alias()
		{
			return _aliases.first() ? _aliases.first() : 0;
		}


		/*****************************
		 ** Name-registry interface **
		 *****************************/

		bool is_unique(const char *name) const
		{
			/* check for name clash with an existing child */
			Genode::List_element<Child> const *curr = first();
			for (; curr; curr = curr->next())
				if (curr->object()->has_name(name))
					return false;

			/* check for name clash with an existing alias */
			for (Alias const *a = _aliases.first(); a; a = a->next()) {
				if (Alias::Name(name) == a->name)
					return false;
			}

			return true;
		}

		Genode::Server *lookup_server(const char *name) const
		{
			/*
			 * Check if an alias with the specified name exists. If so,
			 * look up the server referred to by the alias.
			 */
			for (Alias const *a = _aliases.first(); a; a = a->next())
				if (Alias::Name(name) == a->name)
					name = a->child.string();

			/* look up child with the name */
			Genode::List_element<Child> const *curr = first();
			for (; curr; curr = curr->next())
				if (curr->object()->has_name(name))
					return curr->object()->server();

			return 0;
		}
};


int main(int, char **)
{
	using namespace Init;
	using namespace Genode;

	/* look for dynamic linker */
	try {
		static Rom_connection rom("ld.lib.so");
		Process::dynamic_linker(rom.dataspace());
	} catch (...) { }

	static Service_registry parent_services;
	static Service_registry child_services;
	static Child_registry   children;
	static Cap_connection   cap;

	/*
	 * Signal receiver for config changes
	 */
	Signal_receiver sig_rec;
	Signal_context  sig_ctx;
	config()->sigh(sig_rec.manage(&sig_ctx));

	for (;;) {

		try {
			config_verbose =
				config()->xml_node().attribute("verbose").has_value("yes"); }
		catch (...) { }

		try { determine_parent_services(&parent_services); }
		catch (...) { }

		/* determine default route for resolving service requests */
		Xml_node default_route_node("<empty/>");
		try {
			default_route_node =
			config()->xml_node().sub_node("default-route"); }
		catch (...) { }

		/* create aliases */
		config()->xml_node().for_each_sub_node("alias", [&] (Xml_node alias_node) {

			try {
				children.insert_alias(new (env()->heap()) Alias(alias_node));
			}
			catch (Alias::Name_is_missing) {
				PWRN("Missing 'name' attribute in '<alias>' entry\n"); }
			catch (Alias::Child_is_missing) {
				PWRN("Missing 'child' attribute in '<alias>' entry\n"); }

		});

		/* create children */
		try {
			config()->xml_node().for_each_sub_node("start", [&] (Xml_node start_node) {

				try {
					children.insert(new (env()->heap())
					                Init::Child(start_node, default_route_node,
					                            &children, read_prio_levels_log2(),
					                            read_affinity_space(),
					                            &parent_services, &child_services, &cap));
				}
				catch (Rom_connection::Rom_connection_failed) {
					/*
					 * The binary does not exist. An error message is printed
					 * by the Rom_connection constructor.
					 */
				}
			});

			/* start children */
			children.start();
		}
		catch (Xml_node::Nonexistent_sub_node) {
			PERR("No children to start"); }
		catch (Xml_node::Invalid_syntax) {
			PERR("No children to start"); }
		catch (Init::Child::Child_name_is_not_unique) { }
		catch (Init::Child_registry::Alias_name_is_not_unique) { }

		/*
		 * Respond to config changes at runtime
		 *
		 * If the config gets updated to a new version, we kill the current
		 * scenario and start again with the new config.
		 */

		/* wait for config change */
		sig_rec.wait_for_signal();

		/* kill all currently running children */
		while (children.any()) {
			Init::Child *child = children.any();
			children.remove(child);
			destroy(env()->heap(), child);
		}

		/* remove all known aliases */
		while (children.any_alias()) {
			Init::Alias *alias = children.any_alias();
			children.remove_alias(alias);
			destroy(env()->heap(), alias);
		}

		/* reset knowledge about parent services */
		parent_services.remove_all();

		/* reload config */
		try { config()->reload(); } catch (...) { }
	}

	return 0;
}

