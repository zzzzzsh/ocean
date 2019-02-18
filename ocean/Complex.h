#pragma once

#include <fftw3.h>

 // ¸´Êý¼ÆËã
class Complex {
public:
	Complex();
	Complex(float real, float imaginary);
	Complex(const Complex & other);
	float Real();
	float Imaginary();
	Complex Conjugate();
	Complex operator+(const Complex & rhs);
	Complex operator-(const Complex & rhs);
	Complex operator*(const Complex & rhs) const;
	Complex operator*(float rhs) const;
	Complex & operator+=(const Complex & rhs);
	Complex & operator-=(const Complex & rhs);
	Complex & operator*=(const Complex & rhs);
	Complex & operator*=(float rhs);
private:
	union
	{
		struct
		{
			float m_Real, m_Imaginary;
		};
		float m_Values[2];
		fftwf_complex m_FFTWComplex;
	};
};