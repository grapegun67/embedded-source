#include <stdio.h>
#include <stdlib.h>

int	main()
{
	FILE	*fp;
	int	rc;
	char	buf[16];

	fp = fopen("/dev/sonic_device", "r");
	if (fp == NULL)
	{
		printf("fopen error\n");
		return (-1);
	}

	rc = fread(buf, 1, sizeof(buf), fp);
	printf("[%s]\n", buf);

	printf("result: [%d]\n", atoi(buf));

	fclose(fp);
	return (0);
}
