#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "waycadeployer.h"

#define MAX_IRQNAME_LEN 128
#define MAX_IRQNUM	2048

struct interrupt {
	int name[MAX_IRQNAME_LEN];
	int irqno;
} irqs[MAX_IRQNUM];


static char *trim_leadspace(const char *s)
{
	while (isspace((int)*s))
		s++;

	return (char *)s;
}

static void remove_newline(char *s)
{
	int len = strlen(s);

	if (len > 0 && s[len - 1] == '\n')
		s[len - 1] = '\0';
}

static int irq_dump(char *i_name)
{
	FILE *fp;
	char buf[4096];

	bool dump_all = !i_name;

	fp = fopen("/proc/interrupts", "r");

	printf("     irq       count     %s\n", i_name ? i_name : "ALL");

	while (fgets(buf, sizeof(buf), fp)) {
		char *p = buf;
		int irqno;
		long long count;

		remove_newline(p);
		p = trim_leadspace(p);
		/* skip IPI, NMI etc: "NMI: 20   Non-maskable interrupts" */
		if (!isdigit(*p))
			continue;

		/* format "17: 0  9470 IR-IO-APIC   17-fasteoi   ehci_hcd:usb1, ioc0" */
		p = strchr(buf, ':');
		if (!p)
			continue;

		/* "17" */
		*p = '\0';
		irqno = strtoul(buf, NULL, 10);

		p++;

		/* Sum counts for this IRQ: "0 9470" */
		count = 0;
		while (1) {
			p = trim_leadspace(p);
			if (!isdigit(*p))
				break;
			count += strtoull(p, &p, 10);
		}

		/* name "IR-IO-APIC   17-fasteoi   ehci_hcd:usb1, ioc0" */
		p = trim_leadspace(p);
		if (dump_all || strstr(p, i_name))
			printf("%8d %12lld    %s\n", irqno, count, p);
	}

	fclose(fp);

	return 0;
}

int main(int argc, char **argv)
{
	int i;

	if (argc == 1)		/* dump all interrupts */
		irq_dump(NULL);

	for (i = 1; i < argc; i++)
		irq_dump(argv[i]);

	return 0;
}
