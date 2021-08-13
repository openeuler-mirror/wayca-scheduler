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
#include "wayca-scheduler.h"
#include "wayca_sc_info.h"

typedef int (*topo_elem_build_t)(xmlNodePtr node);
typedef int (*topo_format_t)(xmlDocPtr doc, xmlValidCtxtPtr ctxt,
				xmlDtdPtr topo_dtd);
typedef int (*topo_verify_prop_t)(xmlNodePtr node);
typedef int (*topo_print_prop_t)(xmlNodePtr node);

static int numa_prop_print(xmlNodePtr node);
static int numa_prop_verify(xmlNodePtr node);
static int numa_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt, xmlDtdPtr topo_dtd);
static int numa_elem_build(xmlNodePtr node);
static int sys_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt, xmlDtdPtr topo_dtd);
static int system_elem_build(xmlNodePtr node);
static int pkg_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt, xmlDtdPtr topo_dtd);
static int package_elem_build(xmlNodePtr node);
static int ccl_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt, xmlDtdPtr topo_dtd);
static int ccl_elem_build(xmlNodePtr node);
static int core_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt, xmlDtdPtr topo_dtd);
static int core_elem_build(xmlNodePtr node);
static int core_prop_print(xmlNodePtr node);
static int core_prop_verify(xmlNodePtr node);
static int cpu_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt, xmlDtdPtr topo_dtd);
static int cpu_elem_build(xmlNodePtr node);

/*
 * In order to ensure subsequent expansion, we cannot introduce the concept of
 * order for topo_elem, so that there can be multiple elems in a level in the
 * future.
 */
static struct {
	const char *name;
	topo_elem_build_t elem_build;
	topo_format_t format;
	topo_verify_prop_t prop_verify;
	topo_print_prop_t prop_print;
} topo_elem[] = {
	{"System", system_elem_build, sys_format, NULL, NULL},
	{"Package", package_elem_build, pkg_format, NULL, NULL},
	{"NUMANode", numa_elem_build, numa_format, numa_prop_verify,
		numa_prop_print},
	{"Cluster", ccl_elem_build, ccl_format, NULL, NULL},
	{"Core", core_elem_build, core_format, core_prop_verify,
		core_prop_print},
	{"CPU", cpu_elem_build, cpu_format, NULL, NULL},
};

enum topo_level {
	TOPO_SYS,
	TOPO_PKG,
	TOPO_NUMA,
	TOPO_CCL,
	TOPO_CORE,
	TOPO_CPU,
	TOPO_MAX
};

static const char *numa_prop_list[] = {
	"mem_size", "L3_cache",
};

static const char *core_prop_list[] = {
	"L1i_cache", "L1d_cache", "L2_cache",
};

/*
 * @elem_list: first elem is the elem to be verified, sequence elems is valid
 *             elem for this verified elem.
 * @size: size of this elem_list
 * return:
 *   negative on error
 */
static int add_elem_formater(xmlDocPtr doc, xmlValidCtxtPtr ctxt,
		xmlDtdPtr topo_dtd, const char **elem_list, size_t size)
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
	int ret;
	int i;

	for (i = index * c_elem_nr; i < (index + 1) * c_elem_nr; i++) {
		c_node = xmlNewChild(node, NULL, next_elem, NULL);
		if (!c_node) {
			topo_err("fail to create sub node %d", i);
			return -ENOMEM;
		}

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
	const char *cpu_elem_list[] = {
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
	char *prop;
	int i;

	for (i = 0; i < ARRAY_SIZE(core_prop_list); i++) {
		prop = (char *)xmlGetProp(node, BAD_CAST core_prop_list[i]);
		if (!prop) {
			topo_err("get %s prop %s failed.", node->name,
					core_prop_list[i]);
			return -ENOENT;
		}
		printf("   %s %s", core_prop_list[i], prop);
		xmlFree(prop);
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
	const char *core_elem_list[] = {
		"Core", "CPU",
	};
	int ret;

	ret = add_elem_formater(doc, ctxt, topo_dtd, core_elem_list,
				ARRAY_SIZE(core_elem_list));
	if (ret) {
		topo_err("build core formater fail, ret = %d.", ret);
		return ret;
	}

	xmlAddAttributeDecl(ctxt, topo_dtd, BAD_CAST "Core",
			BAD_CAST "L2_cache", NULL, XML_ATTRIBUTE_CDATA,
			XML_ATTRIBUTE_REQUIRED, NULL, NULL);

	xmlAddAttributeDecl(ctxt, topo_dtd, BAD_CAST "Core",
			BAD_CAST "L1i_cache", NULL, XML_ATTRIBUTE_CDATA,
			XML_ATTRIBUTE_REQUIRED, NULL, NULL);

	xmlAddAttributeDecl(ctxt, topo_dtd, BAD_CAST "Core",
			BAD_CAST "L1d_cache", NULL, XML_ATTRIBUTE_CDATA,
			XML_ATTRIBUTE_REQUIRED, NULL, NULL);
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
	char *index_prop;
	char *endptr;
	int c_nr = 1;
	int core_id;
	int ret;

	index_prop = (char *)xmlGetProp(node, BAD_CAST "index");
	if (!index_prop) {
		topo_err("get core index fail.");
		return -EINVAL;
	}
	errno = 0; /* errno may be a residual value */
	core_id = strtol(index_prop, &endptr, 10);
	if (endptr == index_prop || errno != 0) {
		xmlFree(index_prop);
		if (errno)
			return -errno;
		return -EINVAL;
	}
	xmlFree(index_prop);

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
	const char *ccl_elem_list[] = {
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
	char *index_prop;
	char *endptr;
	int c_nr = 1;
	int ccl_id;
	int ret;

	index_prop = (char *)xmlGetProp(node, BAD_CAST "index");
	if (!index_prop) {
		topo_err("get cluster index fail.");
		return -ENOENT;
	}
	errno = 0; /* errno may be a residual value */
	ccl_id = strtol(index_prop, &endptr, 10);
	if (endptr == index_prop || errno != 0) {
		xmlFree(index_prop);
		if (errno)
			return -errno;
		return -EINVAL;
	}
	xmlFree(index_prop);

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
	const char *numa_elem_list[] = {
		"NUMANode", "Cluster", "Core"
	};
	xmlAttributePtr attr;
	int ret;

	ret = add_elem_formater(doc, ctxt, topo_dtd, numa_elem_list,
			ARRAY_SIZE(numa_elem_list));
	if (ret) {
		topo_err("build numa formater fail, ret = %d.", ret);
		return ret;
	}

	attr = xmlAddAttributeDecl(ctxt, topo_dtd, BAD_CAST "NUMANode",
			BAD_CAST "mem_size", NULL, XML_ATTRIBUTE_CDATA,
			XML_ATTRIBUTE_REQUIRED, NULL, NULL);

	if (!attr) {
		topo_err("add mem_size prop formater fail.");
		return -ENOMEM;
	}

	attr = xmlAddAttributeDecl(ctxt, topo_dtd, BAD_CAST "NUMANode",
			BAD_CAST "L3_cache", NULL, XML_ATTRIBUTE_CDATA,
			XML_ATTRIBUTE_REQUIRED, NULL, NULL);
	if (!attr) {
		topo_err("add L3 cache prop formater fail.");
		return -ENOMEM;
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

static int numa_elem_build(xmlNodePtr node)
{
	const xmlChar *next_elem = BAD_CAST topo_elem[TOPO_CCL].name;
	char *index_prop;
	char *endptr;
	int numa_id;
	int c_nr;
	int ret;

	index_prop = (char *)xmlGetProp(node, BAD_CAST "index");
	if (!index_prop) {
		topo_err("get numa node index fail.");
		return -ENOENT;
	}
	errno = 0; /* errno may be a residual value */
	numa_id = strtol(index_prop, &endptr, 10);
	if (endptr == index_prop || errno != 0) {
		xmlFree(index_prop);
		if (errno)
			return -errno;
		return -EINVAL;
	}
	xmlFree(index_prop);

	ret = numa_prop_build(node, numa_id);
	if (ret) {
		topo_err("build node properties fail, ret = %d.", ret);
		return ret;
	}

	c_nr = wayca_sc_ccls_in_node();
	if (c_nr < 0) {
		topo_warn("number of clusters is invalid, cluster level may not be supported.");
		c_nr = wayca_sc_cpus_in_node();
		if (c_nr < 0) {
			topo_err("number of core is wrong.");
			return c_nr;
		}
		next_elem = BAD_CAST topo_elem[TOPO_CORE].name;
	}

	ret = topo_build_next_elem(node, numa_id, c_nr, next_elem);
	if (ret)
		topo_err("fail to create numa node.");

	return ret;
}

static int pkg_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt,
		xmlDtdPtr topo_dtd)
{
	const char *pkg_elem_list[] = {
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
	char *index_prop;
	int package_id;
	char *endptr;
	int numa_nr;
	int ret;

	numa_nr = wayca_sc_nodes_in_package();
	if (numa_nr < 0) {
		topo_err("number of package is wrong.");
		return numa_nr;
	}

	index_prop = (char *)xmlGetProp(node, BAD_CAST "index");
	if (!index_prop) {
		topo_err("get package index fail.");
		return -ENOENT;
	}
	errno = 0; /* errno may be a residual value */
	package_id = strtol(index_prop, &endptr, 10);
	if (endptr == index_prop || errno != 0) {
		xmlFree(index_prop);
		if (errno)
			return -errno;
		return -EINVAL;
	}
	xmlFree(index_prop);

	ret = topo_build_next_elem(node, package_id, numa_nr, next_elem);
	if (ret)
		topo_err("fail to create package node.");

	return ret;
}

static int sys_format(xmlDocPtr doc, xmlValidCtxtPtr ctxt, xmlDtdPtr topo_dtd)
{
	const char *sys_elem_list[] = {
		"System", "Package",
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
	const xmlChar *next_elem = BAD_CAST topo_elem[TOPO_PKG].name;
	int package_nr;
	int ret;

	package_nr = wayca_sc_packages_in_total();
	if (package_nr < 0) {
		topo_err("number of package is wrong, ret = %d.", package_nr);
		return package_nr;
	}

	ret = topo_build_next_elem(node, 0, package_nr, next_elem);
	if (ret)
		topo_err("fail to create package node.");

	return ret;
}

static int build_topo(xmlNodePtr node)
{
	xmlNodePtr child_node;
	xmlNodePtr cur_node;
	int ret;
	int i;

	for (i = 0; i < ARRAY_SIZE(topo_elem); i++) {
		if (!(strcmp((char *)node->name, topo_elem[i].name)))
			break;
	}

	if (i >= ARRAY_SIZE(topo_elem) || !topo_elem[i].elem_build)
		return 0;

	for (cur_node = node; cur_node;
				cur_node = xmlNextElementSibling(cur_node)) {
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

static int xml_import_topo_info(const char *filename, xmlDocPtr *topo_doc)
{
	int ret;

	ret = access(filename, R_OK);
	if (ret) {
		topo_err("read xml file fail: %s", strerror(errno));
		return -errno;
	}

	*topo_doc = xmlReadFile(filename, "UTF-8", 1);
	if (!*topo_doc) {
		topo_err("parse xml file fail.");
		return -ENOENT;
	}
	return 0;
}

int get_topo_info(struct topo_info_args *args, xmlDocPtr *topo_doc)
{
	int ret;

	if (args->has_input_file)
		ret = xml_import_topo_info(args->input_file_name, topo_doc);
	else
		ret = build_topo_info(topo_doc);
	return ret;
}

static int numa_prop_print(xmlNodePtr node)
{
	char *prop;
	int i;

	for (i = 0; i < ARRAY_SIZE(numa_prop_list); i++) {
		prop = (char *)xmlGetProp(node, BAD_CAST numa_prop_list[i]);
		if (!prop) {
			topo_err("get prop %s failed", numa_prop_list[i]);
			return -ENOENT;
		}
		printf("   %s %s", numa_prop_list[i], prop);
		xmlFree(prop);
	}
	return 0;
}

static int print_prop(xmlNodePtr node)
{
	char *index_prop;
	int ret = 0;
	int i;

	if (strcmp((char *)node->name, topo_elem[TOPO_SYS].name)) {
		index_prop = (char *)xmlGetProp(node, BAD_CAST "index");
		if (!index_prop) {
			topo_err("get index fail.");
			return -ENOENT;
		}
		printf(" #%s", index_prop);
		xmlFree(index_prop);
	}

	for (i = 0; i < ARRAY_SIZE(topo_elem); i++) {
		if (strcmp((char *)node->name, topo_elem[i].name))
			continue;
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
	int ret;

	ret = access(filename, F_OK);
	if (!ret) {
		topo_err("read xml file fail: File exists");
		return -EEXIST;
	}

	ret = xmlSaveFormatFileEnc(filename, topo_doc, "UTF-8", 1);
	if (ret < 0) {
		topo_err("file write fail, ret = %d.", ret);
		return -ENOENT;
	}
	return 0;
}

int put_topo_info(struct topo_info_args *args, xmlDocPtr topo_doc)
{
	int ret;

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

		if (i == 0)
			continue;
		/* register index attribute for each element except System */
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
					core_prop_list[i], prop);
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

