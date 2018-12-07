/**
 * ============================================================================
 * Name        : 12_hp_elliptic_iir.c
 * Author      : Renato Abreu
 * Version     : 0.3
 * Copyright   : Copyright by Renato Abreu
 * Description : BMC verification of digital filters
 * ============================================================================
 */
//#include <assert.h>
//#include <stdio.h>

#define CLOCK		16000000
#define CYCLE		1/CLOCK
#define OVERHEAD	80				//Initialized considering overhead (time for data conditioning, DAC, ADC, etc...)
#define SAMPLERATE 	48000
#define DEADLINE 	1/SAMPLERATE

#define WRAP		0
#define NOWRAP		1
#define SATURATE	2

/*==========================================================================================================================
======================================================= PARAMETERS =========================================================
==========================================================================================================================*/

////Elliptic High Pass from Matlab Fs 48000 Fc 14000, |H|=5.39                       
float a[] = { 1.0f, 2.5371093750000f, 3.9998779296875f, 3.9998779296875f, 2.9063720703125f, 1.2883300781250f, 0.29980468750000f };
float b[] = { 0.0084228515625000f, -0.030395507812500f, 0.058837890625000f, -0.072143554687500f, 0.058837890625000f, -0.030395507812500f, 0.0084228515625000f };
int k = 3; int l = 13;
int Na = 7; int Nb = 7;
float max = 1.0f; float min = -1.0f;
int xsize = 14;

/*==========================================================================================================================
======================================================= FUNCTIONS ==========================================================
==========================================================================================================================*/

float shiftL(float zIn, float z[], int N)
{
	int i;
	float zOut;
	zOut = z[0];
	for (i=0; i<=N-2; i++){
		z[i] = z[i+1];
	}
	z[N-1] = zIn;
	return (zOut);
}

int order(int Na, int Nb)
{
	return Na>Nb ? Na-1 : Nb-1;
}

float abs(float var)
{
	if (var < 0)
		return -var;
	return var;
}

float powInt(int x, int y)
{
	int i;
	float ret = 1.0;
	for(i=0; i<y; ++i)
	{
		ret *= x;
	}
	return ret;
}

int roundInt(float number)
{
	float f = (int)number;
	if(number != f)
	{
		if (number>=0)
			return (int)(number+0.5f);
		else
			return (int)(number-0.5f);
	}
	return (int)number;
}

int ceilInt(float number)
{
	float ret = (number - (int)number > 0.0f) ? (number + 1.0f) : (number);
    return (int)ret;
}

int floorInt(float number)
{
	float ret = (number - (int)number > 0.0f) ? (number) : (number - 1.0f);
    return (int)ret;
}

float maxFixed(int k, int l)
{
	return (float)powInt(2,k-1)-(1.0f/powInt(2,l));
}

float minFixed(int k, int l)
{
	return (-1.0f)*(float)powInt(2,k-1);
}

int wrap(int kX, int kLowerBound, int kUpperBound)  
{
    int range_size = kUpperBound - kLowerBound + 1;

    if (kX < kLowerBound)
        kX += range_size * ((kLowerBound - kX) / range_size + 1);

    return kLowerBound + (kX - kLowerBound) % range_size;
}

float wrapF(float fX, float fLowerBound, float fUpperBound)
{
	int kX = (int)fX;
	int kLowerBound = (int)fLowerBound;
	int kUpperBound = (int)fUpperBound;

	float f = (fX >= 0) ? fX-kX : kX-fX;

	int range_size = kUpperBound - kLowerBound + 1;

    if (kX < kLowerBound)
        kX += range_size * ((kLowerBound - kX) / range_size + 1);

    int ret = (kLowerBound + (kX - kLowerBound) % range_size);
    return (ret >= 0) ? ret + f : ret - f;
}

float floatToFix(float number, float delta, float minFix, float maxFix, int mode)
{
	float div = roundInt(number/delta);
	float fix = delta*div;

	if (mode == NOWRAP)
	{
		assert(fix <= maxFix && fix >= minFix);
	}
	else if (fix > maxFix || fix < minFix)
	{
		if(mode == WRAP)
			fix = wrapF(fix, minFix, maxFix); //wrap around
		else if (fix > maxFix)
			fix = maxFix;
		else
			fix = minFix;
	}
	return fix;
}

float fixedDelta(int l)
{
	return 1.0/powInt(2,l);
}

//Timing data got using WCET analysis of assembly code generated by MSP430 CCS compiler
float iirOutTime(float y[], float x[], float a[], float b[], int Na, int Nb)// timer1 += 8;
{
	int timer1 = OVERHEAD;
	float *a_ptr, *y_ptr, *b_ptr, *x_ptr;									// timer1 += 4;
	float sum = 0;															// timer1 += 5;
	a_ptr = &a[1];															// timer1 += 1;
	y_ptr = &y[Na-1];														// timer1 += 5;
	b_ptr = &b[0];															// timer1 += 1;
	x_ptr = &x[Nb-1];														// timer1 += 6;
	int i, j;
	timer1 += 30;		//(8+4+5+1+5+1+6);
	for (i = 0; i < Nb; i++){												// timer1 += 3;
		sum += *b_ptr++ * *x_ptr--;											// timer1 += (5+3+3+3);
		timer1 += 17;	//(5+3+3+3+3);
	}
	for (j = 1; j < Na; j++){												// timer1 += 2;
		sum -= *a_ptr++ * *y_ptr--;											// timer1 += (5+5+3+1);
		timer1 += 16;	//(5+5+3+1+2);
	}																		// timer1 += 4;
	timer1 += 11;		//(4+4+3);
	assert((double)timer1*CYCLE <= (double)DEADLINE);
	return sum;																// timer1 += 4;
}																			// timer1 += 3;

float iirOutFixed(float y[], float x[], float a[], float b[], int Na, int Nb, float delta, float minFix, float maxFix, int mode)
{
	float *a_ptr, *y_ptr, *b_ptr, *x_ptr;
	float sum = 0;
	a_ptr = &a[1];
	y_ptr = &y[Na-1];
	b_ptr = &b[0];
	x_ptr = &x[Nb-1];
	int i, j;
	for (i = 0; i < Nb; i++){
		sum += floatToFix(*b_ptr++ * *x_ptr--, delta, minFix, maxFix, mode);
		sum = floatToFix(sum, delta, minFix, maxFix, mode);
	}
	for (j = 1; j < Na; j++){
		sum -= floatToFix(*a_ptr++ * *y_ptr--, delta, minFix, maxFix, mode);
		sum = floatToFix(sum, delta, minFix, maxFix, mode);
	}
	return sum;
}

float iirIIOutFixed(float w[], float x, float a[], float b[], int Na, int Nb, float delta, float minFix, float maxFix, int mode)
{
	float *a_ptr, *b_ptr, *w_ptr;
	float sum = 0;
	a_ptr = &a[1];
	b_ptr = &b[0];
	w_ptr = &w[1];
	int k, j;
	for (j = 1; j < Na; j++){
		w[0] -= floatToFix(*a_ptr++ * *w_ptr++, delta, minFix, maxFix, mode);
		w[0] = floatToFix(w[0], delta, minFix, maxFix, mode);
	}
	w[0] += x;
	w_ptr = &w[0];
	for (k = 0; k < Nb; k++){
		sum += floatToFix(*b_ptr++ * *w_ptr++, delta, minFix, maxFix, mode);
		sum = floatToFix(sum, delta, minFix, maxFix, mode);
	}

	return sum;
}

float iirIItOutFixed(float w[], float x, float a[], float b[], int Na, int Nb, float delta, float minFix, float maxFix, int mode)
{
	float *a_ptr, *b_ptr;
	float yout = 0;
	a_ptr = &a[1];
	b_ptr = &b[0];
	int Nw = Na > Nb ? Na : Nb;
	yout = floatToFix((*b_ptr++ * x + w[0]), delta, minFix, maxFix, mode);

	int j;
	for (j = 0; j < Nw-1; j++){
		w[j] = w[j+1];
		if(j<Na-1){
			w[j] -= floatToFix(*a_ptr++ * yout, delta, minFix, maxFix, mode);
			w[j] = floatToFix(w[j], delta, minFix, maxFix, mode);
		}
		if(j<Nb-1){
			w[j] += floatToFix(*b_ptr++ * x, delta, minFix, maxFix, mode);// *b_ptr++ * x;
			w[j] = floatToFix(w[j], delta, minFix, maxFix, mode);
		}
	}

	return yout;
}

int nondet_int();
float nondet_float();

/*==========================================================================================================================
==================================================== OVERFLOW CHECKING =====================================================
==========================================================================================================================*/
int main(void)
{
	int i;

	float delta = fixedDelta(l);
	float minFix = minFixed(k,l);
	float maxFix = maxFixed(k,l);

	// Quantizing coeficients
	for (i=0; i<Na; ++i)
	{
		a[i] = floatToFix(a[i], delta, minFix, maxFix, NOWRAP);
	}
	for (i=0; i<Nb; ++i)
	{
		b[i] = floatToFix(b[i], delta, minFix, maxFix, NOWRAP);
	}

	///////////////////////////// STATES ///////////////////////////////

	float y[xsize];
	float x[xsize];
	for (i = 0; i<xsize; ++i)
	{
		y[i] = 0;
		x[i] = nondet_int()*delta; //nondet_float();
		__ESBMC_assume(x[i]>=min && x[i]<=max);
	}

	float yaux[Na];
	float xaux[Nb];
	for (i = 0; i<Na; ++i)
	{
		yaux[i] = 0;
	}
	for (i = 0; i<Nb; ++i)
	{
		xaux[i] = 0;
	}

	///////////////////////////// FILTER DIRECT FORM I ///////////////////////////////
	for(i=0; i<xsize; ++i)
	{
		shiftL(x[i],xaux,Nb);
		y[i] = iirOutFixed(yaux, xaux ,a ,b ,Na ,Nb, delta, minFix, maxFix, NOWRAP);
		shiftL(y[i],yaux,Na);
	}
	return 0;
}

/*==========================================================================================================================
=================================================== LIMIT CYCLE CHECKING ===================================================
==========================================================================================================================*/
int limitCycle(void)
{
	int i;
	int Set_xsize_at_least_two_times_Na = 2*Na;
	assert(xsize >= Set_xsize_at_least_two_times_Na);

	float delta = fixedDelta(l);	//quantization interval
    float minFix = minFixed(k,l);
	float maxFix = maxFixed(k,l);

	// Quantizing coefficients
	for (i=0; i<Na; ++i)
	{
		a[i] = floatToFix(a[i], delta, minFix, maxFix, NOWRAP);
	}
	for (i=0; i<Nb; ++i)
	{
		b[i] = floatToFix(b[i], delta, minFix, maxFix, NOWRAP);
	}

	///////////////////////////// STATES ///////////////////////////////
	float x[xsize];
	float y[xsize];

	for (i = 0; i<xsize; ++i)
	{
		y[i] = 0;
		x[i] = 0;
	}

	float xaux[Nb];
	float yaux[Na];
	float y0[Na];

	yaux[0] = 0;
	y0[0] = 0;
	for (i = 1; i<Na; ++i)
	{
		yaux[i] = nondet_int() * delta;
		__ESBMC_assume(yaux[i]>=min && yaux[i]<=max);
		y0[i] = yaux[i]; //stores initial value for later comparison
	}

	for (i = 0; i<Nb; ++i)
	{
		xaux[i] = 0;
	}

	int j;
	int count = 0;
	int window = order(Na,Nb);
	int notzeros = 0;

	///////////////////////////// FILTER ///////////////////////////////
	for(i=0; i<xsize; ++i)
	{
		shiftL(x[i],xaux,Nb);
		y[i] = iirOutFixed(yaux, xaux, a, b, Na, Nb, delta, minFix, maxFix, WRAP);
		shiftL(y[i],yaux,Na);

		for(j=Na-1; j>0; --j)
		{
			if(yaux[j] == y0[j])
			{
				++count;
			}
			if(yaux[j] != 0.0f)
			{
				++notzeros;
			}
		}
		if(notzeros != 0)
		{
			assert(count < Na-1);
		}
		count = 0;
		notzeros = 0;
	}

	return 0;
}

/*==========================================================================================================================
=================================================== TIMING VERIFICATION ====================================================
==========================================================================================================================*/
int timing(void)
{
	int i;

	///////////////////////////// STATES ///////////////////////////////
	float x[xsize];
	float y[xsize];
	for (i = 0; i<xsize; ++i){
		y[i] = 0;
		x[i] = nondet_float();
		__ESBMC_assume(x[i]>=min && x[i]<=max);
	}

	float xaux[Nb];
	float yaux[Na];
	for (i = 0; i<Na; ++i){
		yaux[i] = 0;
	}
	for (i = 0; i<Nb; ++i){
		xaux[i] = 0;
	}

	///////////////////////////// FILTER ///////////////////////////////
	for(i=0; i<xsize; ++i){
		shiftL(x[i],xaux,Nb);
		y[i] = iirOutTime(yaux, xaux, a, b, Na, Nb);
		shiftL(y[i],yaux,Na);
	}
	return 0;
}
