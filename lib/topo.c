/*
 * TODO: read the topology from sysfs
 */
int cores_in_ccl(void)
{
	return 4;
}

int cores_in_node(void)
{
	return 24;
}

int cores_in_package(void)
{
	return 48;
}

int cores_in_total(void)
{
	return 96;
}

int nodes_in_package(void)
{
	return 2;
}

int nodes_in_total(void)
{
	return 4;
}
