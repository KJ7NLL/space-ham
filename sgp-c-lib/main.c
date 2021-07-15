#include "sgp.h"
#include <stdio.h>
#include <math.h>

inline void clrscr (void)
{
	register int i;
	for (i=1;(i<25);i++) printf("\n");
}

void gettestdata (struct tle_ascii *sat_data1, struct tle_ascii *sat_data2)
{
	FILE *testdatafile;
	testdatafile=fopen("testdata","r");
	fgets(sat_data1->l[1],70,testdatafile);
	fgets(sat_data1->l[2],70,testdatafile);
	fgets(sat_data2->l[1],70,testdatafile);
	fgets(sat_data2->l[2],70,testdatafile);	
	fclose(testdatafile);
}

void sgp_test (void)
{
	struct vector pos[5], vel[5];
	struct sgp_data satdata;

	int satnumber=0, interval, j;
	double delta, tsince;
	struct tle_ascii sat_data[2];
	gettestdata(&sat_data[0],&sat_data[1]);
	delta = 360.0;
	for (j=0;(j<5);j++)
	{
		int model=0;
		printf("\n\n\n");
		switch (j)
		{
		case 0 : 
			model = _SGP0;
			printf("*** SGP0 ***\n\n");
			satnumber = 0;
			break;
		case 1 :
			model = _SGP4;
			printf("*** SGP4 ***\n\n");
			satnumber = 0;
			break;
		case 2 :
			model = _SDP4;
			printf("*** SDP4 ***\n\n");
			satnumber = 1;
			break;
		case 3 :
			model = _SGP8;
			printf("*** SGP8 ***\n\n");
			satnumber = 0;
			break;
		case 4 :
			model = _SDP8;
			printf("*** SDP8 ***\n\n");
			satnumber = 1;
			break;
		}

		printf("%s",sat_data[satnumber].l[1]);
		printf("%s",sat_data[satnumber].l[2]);
		printf("\n");
		printf("\n");
		Convert_Satellite_Data(sat_data[satnumber],&satdata);
		for (interval=0;(interval<=4);interval++)
		{
			tsince = interval * delta;
			switch (model) 
			{
			case _SGP0:
				sgp0(tsince,&pos[interval],&vel[interval],&satdata);
				break;
			case _SGP4:
				sgp4(tsince,&pos[interval],&vel[interval],&satdata);
				break;
			case _SDP4:
				sdp4(tsince,&pos[interval],&vel[interval],&satdata);
				break;
			case _SGP8:
				sgp8(tsince,&pos[interval],&vel[interval],&satdata);
				break;
			case _SDP8:
				sdp8(tsince,&pos[interval],&vel[interval],&satdata);
				break;
			}
			Convert_Sat_State(&pos[interval],&vel[interval]);
			pos[interval].v[3]=tsince;
		}
		printf("     TSINCE              X                Y                Z \n");
		for (interval=0;(interval<=4);interval++)
			printf(" %4.1f  %8.8f       %8.8f         %8.8f \n", pos[interval].v[3], pos[interval].v[0],pos[interval].v[1],pos[interval].v[2]);
		printf("\n\n\n                     XDOT             YDOT             ZDOT \n");
		for (interval=0;(interval<=4);interval++)
			printf("  %9.8f      %9.8f     %9.8f \n",vel[interval].v[0],vel[interval].v[1],vel[interval].v[2]);
		printf("\n");


	}
}

int main (void)
{
	sgp_test();
       	return 0;
}





