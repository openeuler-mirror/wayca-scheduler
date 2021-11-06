/*
 * Copyright (c) 2021 HiSilicon Technologies Co., Ltd.
 * Wayca scheduler is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 * See the Mulan PSL v2 for more details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <fcntl.h>
#include "wayca-scheduler.h"
#include "wayca_sc_info.h"

typedef int (*topo_elem_build_t)(xmlNodePtr node);
typedef int (*topo_format_t)(xmlDocPtr doc, xmlValidCtxtPtr ctxt,
				xmlDtdPtr topo_dtd);
typedef int (*topo_verify_prop_t)(xmlNodePtr node);
typedef int (*topo_print_prop_t)(xmlNodePtr node);

static int output_irq;
static int output_dev;

static int numa_prop_print(xmlNodePtr node);
static int numa_prop_verify(xmlNodePtr node);
static int numa_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt, xmlDtdPtr topo_dtd);
static int numa_elem_build(xmlNodePtr node);
static int system_elem_build(xmlNodePtr node);
static int sys_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt, xmlDtdPtr topo_dtd);
static int package_elem_build(xmlNodePtr node);
static int pkg_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt, xmlDtdPtr topo_dtd);
static int ccl_elem_build(xmlNodePtr node);
static int ccl_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt, xmlDtdPtr topo_dtd);
static int core_elem_build(xmlNodePtr node);
static int core_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt, xmlDtdPtr topo_dtd);
static int core_prop_verify(xmlNodePtr node);
static int core_prop_print(xmlNodePtr node);
static int cpu_elem_build(xmlNodePtr node);
static int cpu_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt, xmlDtdPtr topo_dtd);
static int intr_elem_build(xmlNodePtr node);
static int intr_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt, xmlDtdPtr topo_dtd);
static int irq_elem_build(xmlNodePtr node);
static int irq_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt, xmlDtdPtr topo_dtd);
static int irq_prop_verify(xmlNodePtr node);
static int irq_prop_print(xmlNodePtr node);
static int pci_dev_elem_build(xmlNodePtr node);
static int pci_dev_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt,
		xmlDtdPtr topo_dtd);
static int pci_prop_verify(xmlNodePtr node);
static int pci_prop_print(xmlNodePtr node);
static int smmu_dev_elem_build(xmlNodePtr node);
static int smmu_dev_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt,
		xmlDtdPtr topo_dtd);
static int smmu_prop_verify(xmlNodePtr node);
static int smmu_prop_print(xmlNodePtr node);
/*
 * In order to ensure subsequent expansion, we cannot introduce the concept of
 * order for topo_elem, so that there can be multiple elems in a level in the
 * future.
 */
static struct {
	const char *name;
	bool has_idx;
	topo_elem_build_t elem_build;
	topo_format_t format;
	topo_verify_prop_t prop_verify;
	topo_print_prop_t prop_print;
} topo_elem[] = {
	{"System", false, system_elem_build, sys_format, NULL, NULL},
	{"Package", true, package_elem_build, pkg_format, NULL, NULL},
	{"NUMANode", true, numa_elem_build, numa_format, numa_prop_verify,
		numa_prop_print},
	{"Cluster", true, ccl_elem_build, ccl_format, NULL, NULL},
	{"Core", true, core_elem_build, core_format, core_prop_verify,
		core_prop_print},
	{"CPU", true, cpu_elem_build, cpu_format, NULL, NULL},
	{"Interrupt", false, intr_elem_build, intr_format, NULL, NULL},
	{"IRQ", false, irq_elem_build, irq_format, irq_prop_verify,
		irq_prop_print},
	{"PCIDEV", false, pci_dev_elem_build, pci_dev_format, pci_prop_verify,
		pci_prop_print},
	{"SMMUDEV", false, smmu_dev_elem_build, smmu_dev_format,
		smmu_prop_verify, smmu_prop_print},
};

enum topo_level {
	TOPO_SYS,
	TOPO_PKG,
	TOPO_NUMA,
	TOPO_CCL,
	TOPO_CORE,
	TOPO_CPU,
	TOPO_INTR,
	TOPO_IRQ,
	TOPO_PCIDEV,
	TOPO_SMMUDEV,
	TOPO_MAX
};

static const char * const numa_prop_list[] = {
	"mem_size", "L3_cache",
};

static const char * const core_prop_list[] = {
	"L1i_cache", "L1d_cache", "L2_cache",
};

static bool is_valid_num(const char *num, int base, long min, long max)
{
	long ret;
	char *endstr;

	errno = 0;
	ret = strtol(num, &endstr, base);
	if (endstr == num || errno != 0)
		return false;
	return *endstr == '\0' && ret >= min && ret <= max;
}

static int print_prop_list(xmlNodePtr node, const char * const *prop_list,
			   int list_size)
{
	char *prop;
	int i;

	for (i = 0; i < list_size; i++) {
		prop = (char *)xmlGetProp(node, BAD_CAST prop_list[i]);
		if (!prop) {
			topo_err("get %s prop %s failed.", node->name,
					prop_list[i]);
			return -ENOENT;
		}
		printf("   %s %s", prop_list[i], prop);
		xmlFree(prop);
	}
	return 0;
}
/*
 * @elem_list: first elem is the elem to be verified, sequence elems is valid
 *             elem for this verified elem.
 * @size: size of this elem_list
 * return:
 *   negative on error
 */
static int add_elem_formater(xmlDocPtr doc, xmlValidCtxtPtr ctxt,
		xmlDtdPtr topo_dtd, const char * const *elem_list, size_t size)
{
	xmlElementContentPtr ancestor_cont;
	xmlElementContentPtr p_cont;
	xmlElementContentPtr c_cont;
	int i;

	if (size < 2) {
		topo_err("not enough element");
		return -EINVAL;
	}

	if (!elem_list[1]) {
		xmlAddElementDecl(ctxt, topo_dtd, BAD_CAST elem_list[0],
					XML_ELEMENT_TYPE_EMPTY, NULL);
		return 0;
	}
	ancestor_cont = xmlNewDocElementContent(doc, BAD_CAST elem_list[1],
					XML_ELEMENT_CONTENT_ELEMENT);
	if (!ancestor_cont)
		return -ENOMEM;
	p_cont = ancestor_cont;

	for (i = 2; i < size; i++) {
		c_cont = xmlNewDocElementContent(doc, BAD_CAST elem_list[i],
				XML_ELEMENT_CONTENT_ELEMENT);
		if (!c_cont)
			return -ENOMEM;
		/*
		 * The content is a tree, if there are several child element
		 * in a element, we need make them a tree
		 */
		p_cont->c2 = c_cont;
		p_cont = c_cont;
	}

	xmlAddElementDecl(ctxt, topo_dtd, BAD_CAST elem_list[0],
				XML_ELEMENT_TYPE_MIXED, ancestor_cont);

	xmlFreeDocElementContent(doc, ancestor_cont);
	return 0;
}

/*
 * @attr_list: first element of the list is the node name, sequence elements
 *             are the attributes which belong to this node.
 * @size: size of this attr_list
 * return:
 *   negative on error
 */
static int add_attr_formater(xmlValidCtxtPtr ctxt, xmlDtdPtr topo_dtd,
				  const char * const *attr_list, size_t size)
{
	xmlAttributePtr attr;
	int i;

	if (size < 2) {
		topo_err("not enough attributes.");
		return -EINVAL;
	}

	for (i = 1; i < size; i++) {
		attr = xmlAddAttributeDecl(ctxt, topo_dtd,
					   BAD_CAST attr_list[0],
					   BAD_CAST attr_list[i], NULL,
					   XML_ATTRIBUTE_CDATA,
					   XML_ATTRIBUTE_REQUIRED, NULL, NULL);

		if (!attr) {
			topo_err("add %s attributes formater for %s fail.",
					attr_list[i], attr_list[0]);
			return -ENOMEM;
		}
	}
	return 0;
}

static int get_index_from_prop(xmlNodePtr node, int *index)
{
	char *index_prop;
	char *endptr;
	int ret = 0;

	index_prop = (char *)xmlGetProp(node, BAD_CAST "index");
	if (!index_prop) {
		topo_err("get %s node index fail.", node->name);
		return -ENOENT;
	}

	errno = 0;
	*index = strtol(index_prop, &endptr, 10);
	if (endptr == index_prop || errno != 0) {
		xmlFree(index_prop);
		if (errno) {
			ret = -errno;
			goto prop_free;
		}
		ret = -EINVAL;
	}

prop_free:
	xmlFree(index_prop);
	return ret;
}

static int build_prop_index(xmlNodePtr node, int id)
{
#define MAX_INT_SIZE 32
	char index[MAX_INT_SIZE] = {0};
	xmlAttrPtr prop;

	snprintf(index, sizeof(index), "%d", id);
	prop = xmlNewProp(node, BAD_CAST "index", BAD_CAST index);
	if (!prop)
		return -ENOMEM;
	return 0;
}

static int topo_build_next_elem(xmlNodePtr node, int index, int c_elem_nr,
		const xmlChar *next_elem)
{
	xmlNodePtr c_node;
	int i, j;
	int ret;

	for (i = index * c_elem_nr; i < (index + 1) * c_elem_nr; i++) {
		c_node = xmlNewChild(node, NULL, next_elem, NULL);
		if (!c_node) {
			topo_err("fail to create sub node %d", i);
			return -ENOMEM;
		}

		for (j = 0; j < ARRAY_SIZE(topo_elem); j++) {
			if (!(strcmp((char *)c_node->name, topo_elem[j].name)))
				break;
		}
		if (j >= ARRAY_SIZE(topo_elem) || !topo_elem[j].has_idx)
			continue;

		ret = build_prop_index(c_node, i);
		if (ret)
			return ret;
	}
	return 0;
}

static bool is_valid_idx(const char *num)
{
#define MAX_CPUS 1280 // kunpeng930 support 16 packects interconnected
	long ret;
	char *endstr;

	ret = strtol(num, &endstr, 10);
	return *endstr == '\0' && ret >= 0 && ret < MAX_CPUS;
}

static bool is_valid_memory_size(const char *mem_size, const char *need_endstr)
{
	char *real_endstr;
	long ret;

	ret = strtol(mem_size, &real_endstr, 10);
	return ret >= 0 && !strcmp(need_endstr, real_endstr);
}

static int cpu_elem_build(xmlNodePtr node)
{
	/* There is no sub node for cpu now , just return */
	return 0;
}

static int cpu_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt, xmlDtdPtr topo_dtd)
{
	static const char * const cpu_elem_list[] = {
		"CPU", NULL,
	};

	int ret;

	ret = add_elem_formater(doc, ctxt, topo_dtd,
				cpu_elem_list, ARRAY_SIZE(cpu_elem_list));
	if (ret) {
		topo_err("build cpu formater fail, ret = %d.", ret);
		return ret;
	}
	return 0;
}

static int core_prop_print(xmlNodePtr node)
{
	int ret;

	ret = print_prop_list(node, core_prop_list, ARRAY_SIZE(core_prop_list));
	if (ret) {
		topo_err("fail to print node %s properties.", node->name);
		return ret;
	}
	return 0;
}

static int core_prop_build(xmlNodePtr numa_node, int core_id)
{
	char content[100] = {0};
	xmlAttrPtr prop;
	int cache_size;

	cache_size = wayca_sc_get_l1i_size(core_id);
	snprintf(content, sizeof(content), "%dKB", cache_size);
	prop = xmlNewProp(numa_node, BAD_CAST"L1i_cache", BAD_CAST content);
	if (!prop)
		return -ENOMEM;

	cache_size = wayca_sc_get_l1d_size(core_id);
	memset(content, 0, sizeof(content));
	snprintf(content, sizeof(content), "%dKB", cache_size);
	prop = xmlNewProp(numa_node, BAD_CAST"L1d_cache", BAD_CAST content);
	if (!prop)
		return -ENOMEM;

	cache_size = wayca_sc_get_l2_size(core_id);
	memset(content, 0, sizeof(content));
	snprintf(content, sizeof(content), "%dKB", cache_size);
	prop = xmlNewProp(numa_node, BAD_CAST"L2_cache", BAD_CAST content);
	if (!prop)
		return -ENOMEM;

	return 0;
}

static int core_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt, xmlDtdPtr topo_dtd)
{
	static const char * const core_elem_list[] = {
		"Core", "CPU",
	};
	static const char * const core_attr_list[] = {
		"Core", "L2_cache", "L1i_cache", "L1d_cache"
	};
	int ret;

	ret = add_elem_formater(doc, ctxt, topo_dtd, core_elem_list,
				ARRAY_SIZE(core_elem_list));
	if (ret) {
		topo_err("build core formater fail, ret = %d.", ret);
		return ret;
	}

	ret = add_attr_formater(ctxt, topo_dtd, core_attr_list,
				     ARRAY_SIZE(core_attr_list));
	if (ret) {
		topo_err("build core attr formater fail, ret = %d.", ret);
		return ret;
	}
	return 0;
}

static int core_prop_verify(xmlNodePtr node)
{
	char *prop;
	int i;

	for (i = 0; i < ARRAY_SIZE(core_prop_list); i++) {
		prop = (char *)xmlGetProp(node, BAD_CAST core_prop_list[i]);
		if (!prop) {
			topo_err("get %s prop %s failed.", node->name,
					core_prop_list[i]);
			return -ENOENT;
		}
		if (!is_valid_memory_size(prop, "KB")) {
			topo_err("%s: invalid %s: %s.", node->name,
					core_prop_list[i], prop);
			xmlFree(prop);
			return -EINVAL;
		}
		xmlFree(prop);
	}
	return 0;
}

static int core_elem_build(xmlNodePtr node)
{
	const xmlChar *next_elem = BAD_CAST topo_elem[TOPO_CPU].name;
	int c_nr = 1;
	int core_id;
	int ret;

	ret = get_index_from_prop(node, &core_id);
	if (ret)
		return ret;

	ret = core_prop_build(node, core_id);
	if (ret) {
		topo_err("build core properties fail, ret = %d.", ret);
		return ret;
	}

	ret = topo_build_next_elem(node, core_id, c_nr, next_elem);
	if (ret)
		topo_err("fail to create core node.");
	return ret;
}

static int ccl_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt, xmlDtdPtr topo_dtd)
{
	static const char * const ccl_elem_list[] = {
		"Cluster", "Core",
	};
	int ret;

	ret = add_elem_formater(doc, ctxt, topo_dtd,
				ccl_elem_list, ARRAY_SIZE(ccl_elem_list));
	if (ret) {
		topo_err("build cluster formater fail, ret = %d.", ret);
		return ret;
	}
	return 0;
}

static int ccl_elem_build(xmlNodePtr node)
{
	const xmlChar *next_elem = BAD_CAST topo_elem[TOPO_CORE].name;
	int c_nr = 1;
	int ccl_id;
	int ret;

	ret = get_index_from_prop(node, &ccl_id);
	if (ret)
		return ret;

	c_nr = wayca_sc_cpus_in_ccl();
	if (c_nr < 0) {
		topo_err("number of core is wrong, ret = %d.", c_nr);
		return c_nr;
	}

	ret = topo_build_next_elem(node, ccl_id, c_nr, next_elem);
	if (ret)
		topo_err("fail to create ccl node.");

	return ret;
}

static int numa_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt, xmlDtdPtr topo_dtd)
{
	static const char * const numa_elem_list[] = {
		"NUMANode", "Cluster", "Core", "PCIDEV", "SMMUDEV"
	};
	static const char * const numa_attr_list[] = {
		"NUMANode", "mem_size", "L3_cache"
	};
	int ret;

	ret = add_elem_formater(doc, ctxt, topo_dtd, numa_elem_list,
			ARRAY_SIZE(numa_elem_list));
	if (ret) {
		topo_err("build numa formater fail, ret = %d.", ret);
		return ret;
	}

	ret = add_attr_formater(ctxt, topo_dtd, numa_attr_list,
				     ARRAY_SIZE(numa_attr_list));
	if (ret) {
		topo_err("build numa node attr formater fail, ret = %d.", ret);
		return ret;
	}
	return 0;
}

static int numa_prop_build(xmlNodePtr numa_node, int numa_id)
{
	char content[100] = {0};
	unsigned long mem_size;
	xmlAttrPtr prop;
	int cache_size;
	int ret;

	ret = wayca_sc_get_node_mem_size(numa_id, &mem_size);
	if (ret) {
		topo_err("fail to get node %d mem size, ret = %d.", numa_id,
				ret);
		return ret;
	}

	snprintf(content, sizeof(content), "%luKB", mem_size);
	prop = xmlNewProp(numa_node, BAD_CAST"mem_size", BAD_CAST content);
	if (!prop)
		return -ENOMEM;

	cache_size = wayca_sc_get_l3_size(numa_id);
	snprintf(content, sizeof(content), "%dKB", cache_size);
	prop = xmlNewProp(numa_node, BAD_CAST"L3_cache", BAD_CAST content);
	if (!prop)
		return -ENOMEM;

	return ret;
}

static int get_dev_elem_type(enum wayca_sc_device_type dev_type,
			     const xmlChar **elem, const xmlChar **attr)
{
	switch (dev_type) {
	case WAYCA_SC_TOPO_DEV_TYPE_PCI:
		*elem = BAD_CAST topo_elem[TOPO_PCIDEV].name;
		*attr = BAD_CAST "slot";
		break;
	case WAYCA_SC_TOPO_DEV_TYPE_SMMU:
		*elem = BAD_CAST topo_elem[TOPO_SMMUDEV].name;
		*attr = BAD_CAST "name";
		break;
	default:
		return -ENOENT;
	}

	return 0;
}

static int numa_dev_elem_build(xmlNodePtr numa_node, int numa_id)
{
	struct wayca_sc_device_info dev_info;
	const xmlChar *next_elem;
	const xmlChar *idx_attr;
	const char **names;
	xmlNodePtr node;
	xmlAttrPtr prop;
	size_t dev_nr;
	int ret;
	int i;

	ret = wayca_sc_get_device_list(numa_id, &dev_nr, NULL);
	if (ret || !dev_nr)
		return ret;

	names = (const char **)calloc(dev_nr, sizeof(const char *));
	if (!names)
		return -ENOMEM;

	ret = wayca_sc_get_device_list(numa_id, &dev_nr, names);
	if (ret)
		goto buffer_free;

	for (i = 0; i < dev_nr; i++) {
		ret = wayca_sc_get_device_info(names[i], &dev_info);
		if (ret)
			goto buffer_free;

		ret = get_dev_elem_type(dev_info.dev_type, &next_elem,
				&idx_attr);
		if (ret)
			goto buffer_free;

		node = xmlNewChild(numa_node, NULL, next_elem, NULL);
		if (!node) {
			topo_err("fail to create device node %s", names[i]);
			ret = -ENOMEM;
			goto buffer_free;
		}

		prop = xmlNewProp(node, idx_attr, BAD_CAST names[i]);
		if (!prop) {
			ret = -ENOMEM;
			goto buffer_free;
		}
	}

buffer_free:
	free(names);
	return ret;
}

static int numa_elem_build(xmlNodePtr node)
{
	const xmlChar *next_elem = BAD_CAST topo_elem[TOPO_CCL].name;
	int numa_id;
	int c_nr;
	int ret;

	ret = get_index_from_prop(node, &numa_id);
	if (ret)
		return ret;

	ret = numa_prop_build(node, numa_id);
	if (ret) {
		topo_err("build node properties fail, ret = %d.", ret);
		return ret;
	}

	c_nr = wayca_sc_ccls_in_node();
	if (c_nr < 0) {
		topo_warn("number of clusters is invalid, cluster level may not be supported.");
		c_nr = wayca_sc_cores_in_node();
		if (c_nr < 0) {
			topo_err("number of core is wrong.");
			return c_nr;
		}
		next_elem = BAD_CAST topo_elem[TOPO_CORE].name;
	}

	ret = topo_build_next_elem(node, numa_id, c_nr, next_elem);
	if (ret) {
		topo_err("fail to create numa node.");
		return ret;
	}

	if (!output_dev)
		return ret;

	ret = numa_dev_elem_build(node, numa_id);
	if (ret)
		topo_err("fail to create device node in numa.");

	return ret;
}

static int pkg_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt,
		xmlDtdPtr topo_dtd)
{
	static const char * const pkg_elem_list[] = {
		"Package", "NUMANode",
	};
	int ret;

	ret = add_elem_formater(doc, ctxt, topo_dtd, pkg_elem_list,
			ARRAY_SIZE(pkg_elem_list));
	if (ret) {
		topo_err("build numa formater fail, ret = %d.", ret);
		return ret;
	}
	return 0;
}

static int package_elem_build(xmlNodePtr node)
{
	const xmlChar *next_elem = BAD_CAST topo_elem[TOPO_NUMA].name;
	int package_id;
	int numa_nr;
	int ret;

	numa_nr = wayca_sc_nodes_in_package();
	if (numa_nr < 0) {
		topo_err("number of package is wrong.");
		return numa_nr;
	}

	ret = get_index_from_prop(node, &package_id);
	if (ret)
		return ret;

	ret = topo_build_next_elem(node, package_id, numa_nr, next_elem);
	if (ret)
		topo_err("fail to create package node.");

	return ret;
}

static int sys_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt, xmlDtdPtr topo_dtd)
{
	static const char * const sys_elem_list[] = {
		"System", "Package", "Interrupt"
	};
	int ret;

	ret = add_elem_formater(doc, ctxt, topo_dtd, sys_elem_list,
				ARRAY_SIZE(sys_elem_list));
	if (ret) {
		topo_err("build numa formater fail, ret = %d.", ret);
		return ret;
	}
	return 0;
}

static int system_elem_build(xmlNodePtr node)
{
	const xmlChar *next_elem;
	int package_nr;
	int ret;

	package_nr = wayca_sc_packages_in_total();
	if (package_nr < 0) {
		topo_err("number of package is wrong, ret = %d.", package_nr);
		return package_nr;
	}

	next_elem = BAD_CAST topo_elem[TOPO_PKG].name;
	ret = topo_build_next_elem(node, 0, package_nr, next_elem);
	if (ret) {
		topo_err("fail to create package node.");
		return ret;
	}

	if (!output_irq)
		return ret;

	next_elem = BAD_CAST topo_elem[TOPO_INTR].name;
	ret = topo_build_next_elem(node, 0, 1, next_elem);
	if (ret)
		topo_err("fail to create interrupts node.");

	return ret;
}

static int build_topo(xmlNodePtr node)
{
	xmlNodePtr child_node;
	xmlNodePtr cur_node;
	int ret;
	int i;

	for (cur_node = node; cur_node;
				cur_node = xmlNextElementSibling(cur_node)) {
		for (i = 0; i < ARRAY_SIZE(topo_elem); i++) {
			if (!(strcmp((char *)cur_node->name, topo_elem[i].name)))
				break;
		}

		if (i >= ARRAY_SIZE(topo_elem) || !topo_elem[i].elem_build)
			return -ENOENT;

		ret = topo_elem[i].elem_build(cur_node);
		if (ret)
			return ret;

		child_node = xmlFirstElementChild(cur_node);
		if (!child_node)
			continue;

		ret = build_topo(child_node);
		if (ret)
			return ret;
	}
	return 0;
}

static int build_topo_info(xmlDocPtr *topo_doc)
{
	xmlNodePtr sys_node;
	xmlDocPtr doc;
	int ret;

	/*
	 * this initialize the library and check potential ABI mismatches
	 * between the version it was compiled for and the actual shared
	 * library used.
	 */
	LIBXML_TEST_VERSION;
	doc = xmlNewDoc(BAD_CAST "1.0");
	if (!doc)
		return -ENOMEM;

	sys_node = xmlNewNode(NULL, BAD_CAST topo_elem[TOPO_SYS].name);
	if (!sys_node) {
		topo_err("fail to create root node.");
		ret = -ENOMEM;
		goto build_fail;
	}
	xmlDocSetRootElement(doc, sys_node);

	ret = build_topo(sys_node);
	if (ret) {
		topo_err("fail to build system topo info.");
		goto build_fail;
	}

	*topo_doc = doc;
	return 0;

build_fail:
	//subtree will be freed too.
	xmlFreeDoc(doc);
	return ret;
}

static xmlNodePtr xml_find_node(xmlNodePtr last_node, const char *node_name)
{
	xmlNode *cur_node;

	if (!last_node)
		return NULL;

	while (last_node) {
		if ((last_node->type == XML_ELEMENT_NODE) &&
				!strcmp((char *)last_node->name, node_name))
			return last_node;

		cur_node = xml_find_node(last_node->children, node_name);
		if (cur_node)
			return cur_node;

		last_node = last_node->next;
	}

	return NULL;
}

static void xml_delete_special_node(xmlDocPtr topo_doc, const char *del_node)
{
	xmlNodePtr root_node;
	xmlNodePtr node;

	root_node = xmlDocGetRootElement(topo_doc);

	node = xml_find_node(root_node, del_node);
	while (node) {
		xmlUnlinkNode(node);
		xmlFreeNode(node);
		node = xml_find_node(root_node, del_node);
	}
}

static int xml_import_topo_info(const char *filename, xmlDocPtr *topo_doc)
{
	*topo_doc = xmlReadFile(filename, "UTF-8", 1);
	if (!*topo_doc) {
		topo_err("parse xml file fail.");
		return -ENOENT;
	}

	if (!output_irq)
		xml_delete_special_node(*topo_doc, "IRQ");
	if (!output_dev) {
		xml_delete_special_node(*topo_doc, "SMMUDEV");
		xml_delete_special_node(*topo_doc, "PCIDEV");
	}
	return 0;
}

int get_topo_info(struct topo_info_args *args, xmlDocPtr *topo_doc)
{
	int ret;

	output_dev = args->output_dev;
	output_irq = args->output_irq;
	if (args->has_input_file)
		ret = xml_import_topo_info(args->input_file_name, topo_doc);
	else
		ret = build_topo_info(topo_doc);
	return ret;
}

static int numa_prop_print(xmlNodePtr node)
{
	int ret;

	ret = print_prop_list(node, numa_prop_list, ARRAY_SIZE(numa_prop_list));
	if (ret) {
		topo_err("fail to print node %s properties.", node->name);
		return ret;
	}
	return 0;
}

static int print_index(xmlNodePtr node)
{
	char *index_prop;

	index_prop = (char *)xmlGetProp(node, BAD_CAST "index");
	if (!index_prop) {
		topo_err("get index fail.");
		return -ENOENT;
	}
	printf(" #%s", index_prop);
	xmlFree(index_prop);
	return 0;
}

static int print_prop(xmlNodePtr node)
{
	int ret = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(topo_elem); i++) {
		if (strcmp((char *)node->name, topo_elem[i].name))
			continue;

		if (topo_elem[i].has_idx) {
			ret = print_index(node);
			if (ret)
				return ret;
		}
		if (!topo_elem[i].prop_print)
			continue;

		ret = topo_elem[i].prop_print(node);
	}
	return ret;
}

static int print_topo_info(int level, xmlNodePtr topo_node)
{
#define MAX_ALIGN_SPACE 256
	char align_space[MAX_ALIGN_SPACE] = {0};
	xmlNodePtr child_node;
	xmlNodePtr cur_node;
	int i = 0;
	int ret;

	while (i < level) {
		strncat(align_space, "    ", sizeof(align_space) - 1);
		i++;
	}

	for (cur_node = topo_node; cur_node;
				cur_node = xmlNextElementSibling(cur_node)) {
		printf("%s", align_space);
		printf("%s", cur_node->name);
		ret = print_prop(cur_node);
		if (ret)
			return ret;

		printf("\n");
		child_node = xmlFirstElementChild(cur_node);
		if (!child_node)
			continue;

		ret = print_topo_info(level + 1, child_node);
		if (ret)
			return ret;
	}
	return 0;
}

static int xml_export_topo_info(const char *filename, xmlDocPtr topo_doc)
{
	xmlChar *xml_buf;
	int buf_size;
	ssize_t cnt;
	int ret = 0;
	int fd;

	fd = open(filename, O_CREAT | O_WRONLY | O_EXCL, 0640);
	if (fd == -1)
		return -errno;

	xmlDocDumpFormatMemoryEnc(topo_doc, &xml_buf, &buf_size, "UTF-8", 1);
	if (xml_buf == NULL) {
		topo_err("dump xml doc to buffer fail.");
		ret = -ENOMEM;
		goto xmldump_fail;
	}

	cnt = write(fd, xml_buf, buf_size);
	if (cnt != buf_size) {
		ret = errno ? -errno : -EIO;
		topo_err("write to file %s fail. ret = %d.", filename, ret);
	}

	xmlFree(xml_buf);
xmldump_fail:
	close(fd);
	return ret;
}

int put_topo_info(struct topo_info_args *args, xmlDocPtr topo_doc)
{
	xmlNodePtr root_node;
	int ret;

	root_node = xmlDocGetRootElement(topo_doc);
	if (!root_node)
		return -EINVAL;

	if (output_dev) {
		if (!xml_find_node(root_node, "PCIDEV"))
			topo_warn("no PCIDEV node present.");
		if (!xml_find_node(root_node, "SMMUDEV"))
			topo_warn("no SMMUDEV node present.");
	}

	if (output_irq) {
		if (!xml_find_node(root_node, "IRQ"))
			topo_warn("no IRQ node present.");
	}

	if (args->has_output_file)
		ret = xml_export_topo_info(args->output_file_name, topo_doc);
	else {
		xmlNodePtr root_node;

		root_node = xmlDocGetRootElement(topo_doc);
		if (!root_node)
			return -ENOENT;
		ret = print_topo_info(0, root_node);
	}
	return ret;
}

static int validate_format(xmlDocPtr topo_doc)
{
	xmlValidCtxtPtr ctxt;
	xmlDtdPtr topo_dtd;
	int ret;
	int i;

	ctxt = xmlNewValidCtxt();
	if (!ctxt) {
		topo_err("create validate context fail.");
		return -ENOMEM;
	}
	topo_dtd = xmlNewDtd(NULL, BAD_CAST "Topo_info", NULL, NULL);
	if (!topo_dtd) {
		topo_err("create topo info dtd failed.");
		xmlFreeValidCtxt(ctxt);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(topo_elem); i++) {
		if (!topo_elem[i].format)
			continue;
		ret = topo_elem[i].format(topo_doc, ctxt, topo_dtd);
		if (ret)
			goto free_format;

		if (!topo_elem[i].has_idx)
			continue;

		xmlAddAttributeDecl(ctxt, topo_dtd, BAD_CAST topo_elem[i].name,
			BAD_CAST "index", NULL, XML_ATTRIBUTE_CDATA,
			XML_ATTRIBUTE_REQUIRED, NULL, NULL);
	}
	/* It will return 1 after successful verification */
	ret = xmlValidateDtd(ctxt, topo_doc, topo_dtd);
	ret = ret == 1 ? 0 : -EINVAL;
free_format:
	xmlFreeValidCtxt(ctxt);
	return ret;
}

static int numa_prop_verify(xmlNodePtr node)
{
	char *prop;
	int i;

	for (i = 0; i < ARRAY_SIZE(numa_prop_list); i++) {
		prop = (char *)xmlGetProp(node, BAD_CAST numa_prop_list[i]);
		if (!prop) {
			topo_err("get %s prop %s failed.", node->name,
					numa_prop_list[i]);
			return -ENOENT;
		}
		if (!is_valid_memory_size(prop, "KB")) {
			topo_err("%s: invalid %s: %s.", node->name,
					numa_prop_list[i], prop);
			xmlFree(prop);
			return -EINVAL;
		}
		xmlFree(prop);
	}
	return 0;
}

static int verify_prop(xmlNodePtr node)
{
	xmlAttrPtr prop;
	char *index;
	int ret = 0;
	int i;

	prop = xmlHasProp(node, BAD_CAST "index");
	if (prop) {
		index = (char *)xmlGetProp(node, BAD_CAST "index");
		if (!index) {
			topo_err("get %s index prop failed", node->name);
			return -ENOENT;
		}
		if (!is_valid_idx(index)) {
			topo_err("%s: invalid index: %s.", node->name, index);
			xmlFree(index);
			return -EINVAL;
		}
		xmlFree(index);
	}

	for (i = 0; i < ARRAY_SIZE(topo_elem); i++) {
		if (strcmp((char *)node->name, topo_elem[i].name))
			continue;
		if (!topo_elem[i].prop_verify)
			continue;

		ret = topo_elem[i].prop_verify(node);
		break;
	}
	return ret;
}

static int verify_topo_value(xmlNodePtr topo_node)
{
	xmlNodePtr child_node;
	xmlNodePtr cur_node;
	int ret;

	for (cur_node = topo_node; cur_node;
			cur_node = xmlNextElementSibling(cur_node)) {
		ret = verify_prop(cur_node);
		if (ret)
			return ret;

		child_node = xmlFirstElementChild(cur_node);
		if (!child_node)
			continue;

		ret = verify_topo_value(child_node);
		if (ret)
			return ret;
	}
	return 0;
}

static int validate_value(xmlDocPtr topo_doc)
{
	xmlNodePtr root_node;
	int ret;

	root_node = xmlDocGetRootElement(topo_doc);
	if (!root_node)
		return -ENOENT;

	ret = verify_topo_value(root_node);
	return ret;
}

int validate_topo_info(xmlDocPtr topo_doc)
{
	int ret;

	ret = validate_format(topo_doc);
	if (ret) {
		topo_err("format is illegal, ret = %d.", ret);
		return ret;
	}

	ret = validate_value(topo_doc);
	if (ret) {
		topo_err("topo info has illegal value, ret = %d.", ret);
		return ret;
	}
	return 0;
}

static int intr_elem_build(xmlNodePtr intr)
{
#define MAX_INT_SIZE 32
	char irq_number[MAX_INT_SIZE] = {0};
	xmlNodePtr node;
	xmlAttrPtr prop;
	uint32_t *irqs;
	size_t irq_nr;
	int ret;
	int i;

	ret = wayca_sc_get_irq_list(&irq_nr, NULL);
	if (ret || !irq_nr)
		return ret;

	irqs = (uint32_t *)calloc(irq_nr, sizeof(uint32_t));
	if (!irqs)
		return -ENOMEM;

	ret = wayca_sc_get_irq_list(&irq_nr, irqs);
	if (ret)
		goto buffer_free;

	for (i = 0; i < irq_nr; i++) {
		node = xmlNewChild(intr, NULL, BAD_CAST "IRQ", NULL);
		if (!node) {
			topo_err("fail to create IRQ node %u", irqs[i]);
			ret = -ENOMEM;
			goto buffer_free;
		}

		snprintf(irq_number, sizeof(irq_number), "%u", irqs[i]);
		prop = xmlNewProp(node, BAD_CAST "irq_number",
					BAD_CAST irq_number);
		if (!prop) {
			ret = -ENOMEM;
			goto buffer_free;
		}
	}

buffer_free:
	free(irqs);
	return ret;
}

static int pci_dev_elem_build(xmlNodePtr pci_node)
{
	struct wayca_sc_device_info dev_info;
	char content[100] = {0};
	const char *pci_slot;
	xmlAttrPtr prop;
	int ret;

	pci_slot = (char *)xmlGetProp(pci_node, BAD_CAST "slot");
	if (!pci_slot) {
		topo_err("get pci slot index fail.");
		return -ENOENT;
	}

	ret = wayca_sc_get_device_info(pci_slot, &dev_info);
	if (ret)
		return ret;

	snprintf(content, sizeof(content), "%d", dev_info.smmu_idx);
	prop = xmlNewProp(pci_node, BAD_CAST"smmu_idx", BAD_CAST content);
	if (!prop)
		return -ENOMEM;

	snprintf(content, sizeof(content), "0x%x", dev_info.class);
	prop = xmlNewProp(pci_node, BAD_CAST"class_id", BAD_CAST content);
	if (!prop)
		return -ENOMEM;

	snprintf(content, sizeof(content), "0x%x", dev_info.vendor);
	prop = xmlNewProp(pci_node, BAD_CAST"vendor_id", BAD_CAST content);
	if (!prop)
		return -ENOMEM;

	snprintf(content, sizeof(content), "0x%x", dev_info.device);
	prop = xmlNewProp(pci_node, BAD_CAST"device_id", BAD_CAST content);
	if (!prop)
		return -ENOMEM;

	snprintf(content, sizeof(content), "%d", dev_info.nb_irq);
	prop = xmlNewProp(pci_node, BAD_CAST"irq_nr", BAD_CAST content);
	if (!prop)
		return -ENOMEM;

	return 0;
}

static int smmu_dev_elem_build(xmlNodePtr smmu_node)
{
	struct wayca_sc_device_info dev_info;
	char content[100] = {0};
	const char *name;
	xmlAttrPtr prop;
	int ret;

	name = (char *)xmlGetProp(smmu_node, BAD_CAST "name");
	if (!name) {
		topo_err("get smmu name fail.");
		return -ENOENT;
	}

	ret = wayca_sc_get_device_info(name, &dev_info);
	if (ret)
		return ret;

	snprintf(content, sizeof(content), "%d", dev_info.smmu_idx);
	prop = xmlNewProp(smmu_node, BAD_CAST"smmu_idx", BAD_CAST content);
	if (!prop)
		return -ENOMEM;

	snprintf(content, sizeof(content), "0x%" PRIx64, dev_info.base_addr);
	prop = xmlNewProp(smmu_node, BAD_CAST"base_addr", BAD_CAST content);
	if (!prop)
		return -ENOMEM;

	snprintf(content, sizeof(content), "%s", dev_info.modalias);
	prop = xmlNewProp(smmu_node, BAD_CAST"modalias", BAD_CAST content);
	if (!prop)
		return -ENOMEM;
	return 0;
}

static int topo_str2ul(const char *str, unsigned long *num)
{
	char *endptr;

	errno = 0;
	*num = strtoul(str, &endptr, 10);
	if (endptr == str || errno != 0) {
		if (errno)
			return -errno;
		return -EINVAL;
	}
	return 0;
}

static const char * const irq_chip_string[] = {
	"invalid",
	"mbigen-v2",
	"ITS-MSI",
	"ITS-pMSI",
	"GICv3",
};

static const char * const irq_type_string[] = {
	"invalid",
	"edge",
	"level",
};

static int irq_prop_build(xmlNodePtr numa_node, unsigned long irq_num)
{
	struct wayca_sc_irq_info irq_info;
	char content[100] = {0};
	xmlAttrPtr prop;
	int ret;

	ret = wayca_sc_get_irq_info(irq_num, &irq_info);
	if (ret) {
		topo_err("failed to get irq information, ret = %d.", ret);
		return ret;
	}

	snprintf(content, sizeof(content), "%s", irq_info.name);
	prop = xmlNewProp(numa_node, BAD_CAST"name", BAD_CAST content);
	if (!prop)
		return -ENOMEM;

	if (irq_info.chip_name >= ARRAY_SIZE(irq_chip_string))
		return -EINVAL;
	snprintf(content, sizeof(content), "%s",
			irq_chip_string[irq_info.chip_name]);
	prop = xmlNewProp(numa_node, BAD_CAST"chip_name", BAD_CAST content);
	if (!prop)
		return -ENOMEM;

	if (irq_info.type >= ARRAY_SIZE(irq_type_string))
		return -EINVAL;
	snprintf(content, sizeof(content), "%s",
			irq_type_string[irq_info.type]);
	prop = xmlNewProp(numa_node, BAD_CAST"type", BAD_CAST content);
	if (!prop)
		return -ENOMEM;

	return ret;
}

static int irq_elem_build(xmlNodePtr node)
{
	unsigned long irq_number;
	char *irq_prop;
	int ret;

	irq_prop = (char *)xmlGetProp(node, BAD_CAST "irq_number");
	if (!irq_prop) {
		topo_err("get irq num fail.");
		return -ENOENT;
	}

	ret = topo_str2ul(irq_prop, &irq_number);
	xmlFree(irq_prop);
	if (ret)
		return ret;

	ret = irq_prop_build(node, irq_number);
	if (ret) {
		topo_err("build irq properties fail, ret = %d.", ret);
		return ret;
	}

	return 0;
}

static int intr_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt, xmlDtdPtr topo_dtd)
{
	static const char * const intr_elem_list[] = {
		"Interrupt", "IRQ",
	};
	int ret;

	ret = add_elem_formater(doc, ctxt, topo_dtd,
				intr_elem_list, ARRAY_SIZE(intr_elem_list));
	if (ret) {
		topo_err("build interrupts formater fail, ret = %d.", ret);
		return ret;
	}
	return 0;
}

static int pci_dev_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt,
			  xmlDtdPtr topo_dtd)
{
	static const char * const pci_elem_list[] = {
		"PCIDEV",
		NULL,
	};
	static const char * const pci_attr_list[] = {
		"PCIDEV",
		"slot",
		"smmu_idx",
		"class_id",
		"vendor_id",
		"device_id",
		"irq_nr"
	};
	int ret;

	ret = add_elem_formater(doc, ctxt, topo_dtd,
				pci_elem_list, ARRAY_SIZE(pci_elem_list));
	if (ret) {
		topo_err("build pci elem formater fail, ret = %d.", ret);
		return ret;
	}

	ret = add_attr_formater(ctxt, topo_dtd, pci_attr_list,
				     ARRAY_SIZE(pci_attr_list));
	if (ret) {
		topo_err("build pci attr formater fail, ret = %d.", ret);
		return ret;
	}

	return 0;
}

static int smmu_dev_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt, xmlDtdPtr topo_dtd)
{
	static const char * const smmu_elem_list[] = {
		"SMMUDEV",
		NULL,
	};
	static const char * const smmu_attr_list[] = {
		"SMMUDEV",
		"name",
		"smmu_idx",
		"base_addr",
		"modalias",
	};
	int ret;

	ret = add_elem_formater(doc, ctxt, topo_dtd,
				smmu_elem_list, ARRAY_SIZE(smmu_elem_list));
	if (ret) {
		topo_err("build pci elem formater fail, ret = %d.", ret);
		return ret;
	}

	ret = add_attr_formater(ctxt, topo_dtd, smmu_attr_list,
				     ARRAY_SIZE(smmu_attr_list));
	if (ret) {
		topo_err("build pci attr formater fail, ret = %d.", ret);
		return ret;
	}

	return 0;
}
static int irq_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt, xmlDtdPtr topo_dtd)
{
	static const char * const irq_elem_list[] = {
		"IRQ", NULL,
	};
	static const char * const irq_attr_list[] = {
		"IRQ",
		"irq_number",
		"name",
		"type",
		"chip_name"
	};
	int ret;

	ret = add_elem_formater(doc, ctxt, topo_dtd,
				irq_elem_list, ARRAY_SIZE(irq_elem_list));
	if (ret) {
		topo_err("build irq formater fail, ret = %d.", ret);
		return ret;
	}

	ret = add_attr_formater(ctxt, topo_dtd, irq_attr_list,
				     ARRAY_SIZE(irq_attr_list));
	if (ret) {
		topo_err("build irq attr formater fail, ret = %d.", ret);
		return ret;
	}
	return 0;
}

static int irq_prop_print(xmlNodePtr node)
{
	static const char * const irq_prop_list[] = {
		"irq_number", "type", "chip_name", "name"
	};
	int ret;

	ret = print_prop_list(node, irq_prop_list, ARRAY_SIZE(irq_prop_list));
	if (ret) {
		topo_err("fail to print node %s properties.", node->name);
		return ret;
	}
	return 0;
}

static int pci_prop_print(xmlNodePtr node)
{
	static const char * const pci_prop_list[] = {
		"slot",
		"smmu_idx",
		"class_id",
		"vendor_id",
		"device_id",
		"irq_nr"
	};

	int ret;

	ret = print_prop_list(node, pci_prop_list, ARRAY_SIZE(pci_prop_list));
	if (ret) {
		topo_err("fail to print node %s properties.", node->name);
		return ret;
	}
	return 0;
}

static int smmu_prop_print(xmlNodePtr node)
{
	static const char * const smmu_prop_list[] = {
		"name", "smmu_idx", "base_addr", "modalias"
	};

	int ret;

	ret = print_prop_list(node, smmu_prop_list, ARRAY_SIZE(smmu_prop_list));
	if (ret) {
		topo_err("fail to print node %s properties.", node->name);
		return ret;
	}
	return 0;
}

static int irq_prop_verify(xmlNodePtr node)
{
	char *prop;
	int ret = 0;

	prop = (char *)xmlGetProp(node, BAD_CAST "irq_number");
	if (!prop) {
		topo_err("get %s prop %s failed.", node->name, "irq_number");
		return -ENOENT;
	}

	if (!is_valid_num(prop, WAYCA_SC_INFO_DEC_BASE, 0, UINT32_MAX)) {
		topo_err("get %s prop %s failed.", node->name, "irq_number");
		ret = -EINVAL;
		goto prop_free;
	}

prop_free:
	xmlFree(prop);
	return ret;
}

static int pci_prop_verify(xmlNodePtr node)
{
	char *prop;
	int ret = 0;

	prop = (char *)xmlGetProp(node, BAD_CAST "smmu_idx");
	if (!prop) {
		topo_err("get %s prop %s failed.", node->name, "smmu_idx");
		return -ENOENT;
	}

	if (!is_valid_num(prop, WAYCA_SC_INFO_DEC_BASE, -1, UINT8_MAX)) {
		topo_err("get %s prop %s failed.", node->name, "smmu_idx");
		ret = -EINVAL;
		goto prop_free;
	}

prop_free:
	xmlFree(prop);
	return ret;
}

static int smmu_prop_verify(xmlNodePtr node)
{
	char *prop;
	int ret = 0;

	prop = (char *)xmlGetProp(node, BAD_CAST "smmu_idx");
	if (!prop) {
		topo_err("get %s prop %s failed.", node->name, "smmu_idx");
		return -ENOENT;
	}

	if (!is_valid_num(prop, WAYCA_SC_INFO_DEC_BASE, -1, UINT8_MAX)) {
		topo_err("get %s prop %s failed.", node->name, "smmu_idx");
		ret = -EINVAL;
		goto prop_free;
	}

prop_free:
	xmlFree(prop);
	return ret;
}
