#include <stdlib.h>

void init_filename_generator(void)
{
	// initialize random generator to fixed seed
	srandom(1);
}

void get_filename(char name[])
{
	int i;
	long int rnd = random();
	for (i = 0; i < 4; ++i)
	{
		int index = rnd % 32;
		rnd /= 32;
		if (index < 26)
			name[i] = index + 'a';
		else
			name[i] = index + '0';
	}

	name[4] = 0;
}


