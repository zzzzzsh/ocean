#include "pch.h"
#include "WaterFFT.h"
#include <glm/glm.hpp>
#include <stb_image.h>

#define EPSILON 1.0e-4f
#define DELTA 1.0e-1
#define PI 3.14159265358979323846264338f
#define TAU 6.28318530718f
#define INDICIES_PER_QUAD 6
#define MIN_DX_DZ 0.02f

float Lerp(float a, float b, float t)
{
	return a + (b - a) * t;
}

float QuadLerp(float a, float b, float c, float d, float tx, float ty)
{
	float ab = Lerp(a, b, tx);
	float cd = Lerp(c, d, tx);
	return Lerp(ab, cd, ty);
}

int Clamp(int min, int max, int value)
{
	return glm::min(max, glm::max(min, value));
}

inline float UniformRandom()
{
	float rand_value = (float)rand() / (float)RAND_MAX;
	return rand_value;
}

Complex NormalComplexRandom()
{
	float x1, x2, w;
	do {
		x1 = 2.0f * UniformRandom() - 1.0f;
		x2 = 2.0f * UniformRandom() - 1.0f;
		w = x1 * x1 + x2 * x2;
	} while (w >= 1.0f);
	w = sqrt((-2.0f * log(w)) / w);
	float rand1 = x1 * w;
	float rand2 = x2 * w;
	return Complex(rand1, rand2);
}

// WaterFFT

WaterFFT::IntensityMap::IntensityMap(const std::string & filename) :
	m_InstensityFile(filename)
{
	m_Data = stbi_load(filename.c_str(), &m_Width, &m_Height,
		&m_Channels, 1);
	m_MaxX = static_cast<uint>(m_Width - 1);
	m_MaxY = static_cast<uint>(m_Height - 1);
}

WaterFFT::IntensityMap::~IntensityMap()
{
	stbi_image_free(m_Data);
}

float WaterFFT::IntensityMap::GetIntensity(float x, float z)
{
	// finding the pixel location in the texture
	float xi_f = x * m_Width;
	float yi_f = z * m_Height;

	uint xi0 = static_cast<uint>(xi_f);
	uint xi1 = glm::min(xi0 + 1, m_MaxX);
	uint yi0 = static_cast<uint>(yi_f);
	uint yi1 = glm::min(yi0 + 1, m_MaxY);

	// Getting the interpolation parameters for x / y directions
	float x_t = xi_f - static_cast<float>(xi0);
	float y_t = yi_f - static_cast<float>(yi0);

	uint ai = yi0 * m_Width + xi0;
	uint bi = yi0 * m_Width + xi1;
	uint ci = yi1 * m_Width + xi0;
	uint di = yi1 * m_Width + xi1;

	float a = static_cast<float>(m_Data[ai]) / 255.0f;
	float b = static_cast<float>(m_Data[bi]) / 255.0f;
	float c = static_cast<float>(m_Data[ci]) / 255.0f;
	float d = static_cast<float>(m_Data[di]) / 255.0f;
	return QuadLerp(a, b, c, d, x_t, y_t);
}

WaterFFT::WaterFFT(unsigned grid_dimension, float meter_dimension,
	unsigned expansion, bool use_fft = true) :
	m_HeightScale(1.0f), m_DisplaceScale(1.0f),
	m_XLength(meter_dimension), m_ZLength(meter_dimension),
	m_Amplitude(0.00005f), m_Gravity(9.81f), m_Wind(32.0f, 32.0f),
	m_IMap(nullptr)
{
	// Check for errors before continuing. First check that the grid dimension
	// passed in is a power of 2.
	unsigned gd_check = grid_dimension;
	while (gd_check > 2)
	{
		if (gd_check % 2 != 0) {
			WaterFFTError error(WaterFFTError::INVALID_GRID_DIM, "The water grid's"
				" dimension must be a power of 2");
			throw(error);
		}
		gd_check /= 2;
	}

	// Check that the ratio of dimension over stride is greater than some minimum
	// value.
	float dx_dz = meter_dimension / (float)m_XStride;
	if (dx_dz < MIN_DX_DZ)
	{
		WaterFFTError error(WaterFFTError::SMALL_DX_DZ, "The dimension in meters"
			" divided by the dimension in grid units should be larger than 2 cm");
		throw(error);
	}

	// Set up the strides for the complete mesh and the part of the mesh that the
	// fft will be used to compute positions for.
	m_XStride = grid_dimension + 1;
	m_ZStride = grid_dimension + 1;
	m_NumVerts = m_XStride * m_ZStride;
	m_fft_XStride = grid_dimension;
	m_fft_ZStride = grid_dimension;
	m_fft_NumVerts = m_fft_XStride * m_fft_ZStride;

	// Allocating arrays for FFTW input and output. 
	uint num_fft_bytes = sizeof(Complex) * m_fft_NumVerts;
	m_HTildeIn = (Complex *)fftwf_malloc(num_fft_bytes);
	m_HTildeSlopeXIn = (Complex *)fftwf_malloc(num_fft_bytes);
	m_HTildeSlopeZIn = (Complex *)fftwf_malloc(num_fft_bytes);
	m_HTildeDisplaceXIn = (Complex *)fftwf_malloc(num_fft_bytes);
	m_HTildeDisplaceZIn = (Complex *)fftwf_malloc(num_fft_bytes);
	m_HTildeOut = (Complex *)fftwf_malloc(num_fft_bytes);
	m_HTildeSlopeXOut = (Complex *)fftwf_malloc(num_fft_bytes);
	m_HTildeSlopeZOut = (Complex *)fftwf_malloc(num_fft_bytes);
	m_HTildeDisplaceXOut = (Complex *)fftwf_malloc(num_fft_bytes);
	m_HTildeDisplaceZOut = (Complex *)fftwf_malloc(num_fft_bytes);

	// Creating FFTW plans.
	m_HTildeFFTWPlan = fftwf_plan_dft_2d(m_fft_XStride, m_fft_ZStride,
		(fftwf_complex *)m_HTildeIn,
		(fftwf_complex *)m_HTildeOut,
		FFTW_FORWARD, FFTW_MEASURE);
	m_HTildeSlopeXPlan = fftwf_plan_dft_2d(m_fft_XStride, m_fft_ZStride,
		(fftwf_complex *)m_HTildeSlopeXIn,
		(fftwf_complex *)m_HTildeSlopeXOut,
		FFTW_FORWARD, FFTW_MEASURE);
	m_HTildeSlopeZPlan = fftwf_plan_dft_2d(m_fft_XStride, m_fft_ZStride,
		(fftwf_complex *)m_HTildeSlopeZIn,
		(fftwf_complex *)m_HTildeSlopeZOut,
		FFTW_FORWARD, FFTW_MEASURE);
	m_HTildeDisplaceXPlan = fftwf_plan_dft_2d(m_fft_XStride, m_fft_ZStride,
		(fftwf_complex *)m_HTildeDisplaceXIn,
		(fftwf_complex *)m_HTildeDisplaceXOut,
		FFTW_FORWARD, FFTW_MEASURE);
	m_HTildeDisplaceZPlan = fftwf_plan_dft_2d(m_fft_XStride, m_fft_ZStride,
		(fftwf_complex *)m_HTildeDisplaceZIn,
		(fftwf_complex *)m_HTildeDisplaceZOut,
		FFTW_FORWARD, FFTW_MEASURE);

	// Initializing all of the buffers needed for the water.
	InitializeVertexBuffer();
	InitializeIndexBuffer();
	InitializeOffsetBuffer(expansion);
}

WaterFFT::~WaterFFT()
{
	// freeing FFTW in and out arrays
	fftwf_free(m_HTildeIn);
	fftwf_free(m_HTildeSlopeXIn);
	fftwf_free(m_HTildeSlopeZIn);
	fftwf_free(m_HTildeDisplaceXIn);
	fftwf_free(m_HTildeDisplaceZIn);
	fftwf_free(m_HTildeOut);
	fftwf_free(m_HTildeSlopeXOut);
	fftwf_free(m_HTildeSlopeZOut);
	fftwf_free(m_HTildeDisplaceXOut);
	fftwf_free(m_HTildeDisplaceZOut);
	RemoveIntensityMap();
}

bool WaterFFT::UseIntensityMap(const std::string & filename)
{
	RemoveIntensityMap();
	m_IMap = new IntensityMap(filename);
	return true;
}

bool WaterFFT::RemoveIntensityMap()
{
	if (m_IMap)
	{
		delete m_IMap;
		m_IMap = nullptr;
		return true;
	}
	return false;
}

std::pair<float, glm::vec3> WaterFFT::HeightNormalAtLocation(
	const glm::vec2 & location, float time)
{
	std::pair<float, glm::vec3> result;
	result = GetLocationHeightNormalFFT(location);
	return result;
}

float WaterFFT::HeightAtLocation(const glm::vec2 & location)
{
	MeshPosition mp = LocationToMeshPosition(location);
	return GetLocationHeightFFT(mp);
}

void WaterFFT::Update(float time)
{
	UpdateFFT(time);
}

void WaterFFT::SwapBuffers()
{
	std::vector<Vertex> * temp = m_ReadBuffer;
	m_ReadBuffer = m_WriteBuffer;
	m_WriteBuffer = temp;
}

const void * WaterFFT::VertexBuffer()
{
	return (void *)m_ReadBuffer->data();
}

const void * WaterFFT::IndexBuffer()
{
	return (void *)m_IndexBuffer.data();
}

const void * WaterFFT::OffsetBuffer()
{
	return (void *)m_OffsetBuffer.data();
}

unsigned WaterFFT::VertexBufferSizeBytes()
{
	return m_ReadBuffer->size() * sizeof(Vertex);
}

unsigned WaterFFT::IndexBufferSizeBytes()
{
	return m_IndexBuffer.size() * sizeof(unsigned int);
}

unsigned WaterFFT::IndexBufferSize()
{
	return m_IndexBuffer.size();
}

unsigned WaterFFT::OffsetBufferSizeBytes()
{
	return m_OffsetBuffer.size() * sizeof(Offset);
}

unsigned WaterFFT::OffsetBufferSize()
{
	return m_OffsetBuffer.size();
}

void WaterFFT::UpdateFFT(float time)
{
	unsigned fft_vertex_index = 0;
	for (unsigned z = 0; z < m_fft_ZStride; ++z)
	{
		float m = z - (m_fft_ZStride / 2.0f);
		float kz = (TAU * m) / m_ZLength;
		for (unsigned x = 0; x < m_fft_XStride; ++x)
		{
			float n = x - (m_fft_XStride / 2.0f);
			float kx = (TAU * n) / m_XLength;
			glm::vec2 k(kx, kz);
			float k_magnitude = glm::length(k);
			// calculate htilde / fourier domain
			const Complex & htilde0 =
				m_VertexExtrasBuffer[fft_vertex_index].m_HTilde0;
			const Complex & htilde0_conj =
				m_VertexExtrasBuffer[fft_vertex_index].m_HTilde0Conjugate;
			Complex htilde = HTilde(htilde0, htilde0_conj, k, time);
			// use htilde to set values for fft computation
			m_HTildeIn[fft_vertex_index] = htilde;
			m_HTildeSlopeXIn[fft_vertex_index] = htilde * Complex(0, kx);
			m_HTildeSlopeZIn[fft_vertex_index] = htilde * Complex(0, kz);
			if (k_magnitude < EPSILON)
			{
				m_HTildeDisplaceXIn[fft_vertex_index] = Complex(0.0f, 0.0f);
				m_HTildeDisplaceZIn[fft_vertex_index] = Complex(0.0f, 0.0f);
			}
			else
			{
				m_HTildeDisplaceXIn[fft_vertex_index] =
					htilde * Complex(0.0f, -kx / k_magnitude);
				m_HTildeDisplaceZIn[fft_vertex_index] =
					htilde * Complex(0.0f, -kz / k_magnitude);
			}
			++fft_vertex_index;
		}
	}

	// Execute the fft.
	fftwf_execute(m_HTildeFFTWPlan);
	fftwf_execute(m_HTildeSlopeXPlan);
	fftwf_execute(m_HTildeSlopeZPlan);
	fftwf_execute(m_HTildeDisplaceXPlan);
	fftwf_execute(m_HTildeDisplaceZPlan);

	// Use the output from the fft for the new vertex positions of the mesh.
	unsigned vertex_index = 0;
	fft_vertex_index = 0;
	int sign = 1;
	for (unsigned z = 0; z < m_fft_ZStride; ++z)
	{
		for (unsigned x = 0; x < m_fft_XStride; ++x)
		{
			// Apply sign to all fft output.
			m_HTildeOut[fft_vertex_index] *= (float)sign;
			m_HTildeSlopeXOut[fft_vertex_index] *= (float)sign;
			m_HTildeSlopeZOut[fft_vertex_index] *= (float)sign;
			m_HTildeDisplaceXOut[fft_vertex_index] *= (float)sign;
			m_HTildeDisplaceZOut[fft_vertex_index] *= (float)sign;

			// Get the starting values for the vertices new position. 
			Vertex & vert = (*m_WriteBuffer)[vertex_index];
			float x_location = m_VertexExtrasBuffer[fft_vertex_index].m_Ox;
			float z_location = m_VertexExtrasBuffer[fft_vertex_index].m_Oz;
			float position_y_factor = m_HeightScale;
			float normal_y_factor = 1.0f / m_HeightScale;

			// Get the factors that need to be applied from the intensity map.
			if (m_IMap)
			{
				float x_0to1 = x / static_cast<float>(m_fft_XStride);
				float z_0to1 = z / static_cast<float>(m_fft_XStride);
				float intensity = m_IMap->GetIntensity(x_0to1, z_0to1);
				if (intensity == 0.0f)
					intensity = EPSILON;
				position_y_factor *= intensity;
				normal_y_factor *= 1.0f / intensity;
			}

			// Set the new position of the vertex.
			vert.m_Px = x_location +
				m_DisplaceScale * m_HTildeDisplaceXOut[fft_vertex_index].Real();
			vert.m_Py = m_HTildeOut[fft_vertex_index].Real() * position_y_factor;
			vert.m_Pz = z_location + m_DisplaceScale *
				m_HTildeDisplaceZOut[fft_vertex_index].Real();

			// Set the new normal of the vertex. 
			glm::vec3 normal(0.0f - m_HTildeSlopeXOut[fft_vertex_index].Real(),
				1.0f, 0.0f - m_HTildeSlopeZOut[fft_vertex_index].Real());
			vert.m_Nx = normal.x;
			vert.m_Ny = normal.y * normal_y_factor;
			vert.m_Nz = normal.z;

			++vertex_index;
			++fft_vertex_index;
			sign *= -1;
		}

		// We need to increment the vertex index again because the number of
		// vertices in the actual vertex array is extended by one in both the x
		// and z direction when compared to the arrays used for computing the fft.
		++vertex_index;
		sign *= -1;
	}

	// Updating the last row and column. These vertices are "attached" to the
	// other side of the grid, so their heights should be equivalent. The first
	// call updates the last vertices in the x direction. The second call updates
	// the last vertices in the z direction.
	UpdateTailEdge(0);
	UpdateTailEdge(1);

	// Now the tail corner is updated. This is a special case because the
	// original vertex that is used is diagonally 
	// opposite from the vertex that is being updated.
	Vertex & update_vert = (*m_WriteBuffer)[m_NumVerts - 1];
	Vertex & og_vert = (*m_WriteBuffer)[0];
	update_vert.m_Px = og_vert.m_Px + m_XLength;
	update_vert.m_Py = og_vert.m_Py;
	update_vert.m_Pz = og_vert.m_Pz + m_ZLength;
}

void WaterFFT::UpdateTailEdge(char edge)
{
	// These values will be different depending on the edge that we choose to
	// update.
	unsigned up_vertex_index;
	unsigned og_vertex_index;
	unsigned d_vertex_index;
	float x_offset;
	float z_offset;

	// If the tail to be updated is zero, we update the vertices at the end of the
	// grid in the x direction. If it is not zero, we update the vertices at the
	// end of the grid in the z direction.
	if (edge == 0)
	{
		up_vertex_index = m_XStride - 1;
		og_vertex_index = 0;
		d_vertex_index = m_XStride;
		x_offset = m_XLength;
		z_offset = 0.0f;
	}
	else
	{
		up_vertex_index = m_XStride * (m_ZStride - 1);
		og_vertex_index = 0;
		d_vertex_index = 1;
		x_offset = 0.0f;
		z_offset = m_ZLength;
	}

	for (unsigned i = 0; i < (m_ZStride - 1); ++i)
	{
		// The vertex being updated is the very last vertex (an edge vertex). The
		// vertex that this updated vertex is attached to is the vextex on the other
		// side of the grid. This other vertex is the original (og) vertex.
		Vertex & update_vert = (*m_WriteBuffer)[up_vertex_index];
		Vertex & og_vert = (*m_WriteBuffer)[og_vertex_index];

		// Update the vertex position.
		update_vert.m_Px = og_vert.m_Px + x_offset;
		update_vert.m_Py = og_vert.m_Py;
		update_vert.m_Pz = og_vert.m_Pz + z_offset;

		// Update the vertices normal.
		update_vert.m_Nx = og_vert.m_Nx;
		update_vert.m_Ny = og_vert.m_Ny;
		update_vert.m_Nz = og_vert.m_Nz;

		// Move the vertex indices forward.
		up_vertex_index += d_vertex_index;
		og_vertex_index += d_vertex_index;
	}
}

std::pair<float, glm::vec3> WaterFFT::GetLocationHeightNormalFFT(
	const glm::vec2 & location)
{
	MeshPosition mp = LocationToMeshPosition(location);
	float height = GetLocationHeightFFT(mp);
	glm::vec3 normal = GetLocationNormalFFT(mp);
	return std::pair<float, glm::vec3>(height, normal);
}

// This relates to the next two functions
/* a---b                  **
** | / |                  **
** c---d                  **
** horizontal goes over x **
** vertical goes over z   */

float WaterFFT::GetLocationHeightFFT(const MeshPosition & mp)
{
	const Vertex & a = (*m_ReadBuffer)[mp.m_VertexIndex];
	const Vertex & b = (*m_ReadBuffer)[mp.m_VertexIndex + 1];
	const Vertex & c = (*m_ReadBuffer)[mp.m_VertexIndex + m_XStride];
	const Vertex & d = (*m_ReadBuffer)[mp.m_VertexIndex + m_XStride + 1];
	//heights
	float ha = a.m_Py;
	float hb = b.m_Py;
	float hc = c.m_Py;
	float hd = d.m_Py;
	//performing bilinear interpolation
	float hab = ha + (hb - ha) * mp.m_Xt; // use lerp formual
	float hcd = hc + (hd - hc) * mp.m_Xt;
	float habcd = hab + (hcd - hab) * mp.m_Zt;
	return habcd;
}

glm::vec3 WaterFFT::GetLocationNormalFFT(const MeshPosition & mp)
{
	const Vertex & a = (*m_ReadBuffer)[mp.m_VertexIndex];
	const Vertex & b = (*m_ReadBuffer)[mp.m_VertexIndex + 1];
	const Vertex & c = (*m_ReadBuffer)[mp.m_VertexIndex + m_XStride];
	const Vertex & d = (*m_ReadBuffer)[mp.m_VertexIndex + m_XStride + 1];
	// getting normals
	glm::vec3 na(a.m_Nx, a.m_Ny, a.m_Nz);
	glm::vec3 nb(b.m_Nx, b.m_Ny, b.m_Nz);
	glm::vec3 nc(c.m_Nx, c.m_Ny, c.m_Nz);
	glm::vec3 nd(d.m_Nx, d.m_Ny, d.m_Nz);
	// normal bilinear interpolation
	glm::vec3 nab = na + (nb - na) * mp.m_Xt; // use lerp formual
	glm::vec3 ncd = nc + (nd - nc) * mp.m_Xt;
	glm::vec3 nabcd = nab + (ncd - nab) * mp.m_Zt;
	nabcd = glm::normalize(nabcd);
	return nabcd;
}

// TODO: This implementation will not work with displacements
// It will only work with a displacement factor of 0
WaterFFT::MeshPosition WaterFFT::LocationToMeshPosition(glm::vec2 location)
{
	// translating to our indicies
	// TODO: remove the requirement (stride = lenght)
	float x_index_float = location.x + (float)m_XStride / 2.0f;
	float z_index_float = location.y + (float)m_ZStride / 2.0f;
	// getting to an index that exists in our arrays
	float x_max_index = (float)(m_XStride - 1);
	if (x_index_float >= x_max_index) {
		unsigned grids_to_offset = (unsigned)(x_index_float / x_max_index);
		x_index_float -= (float)(grids_to_offset * x_max_index);
	}
	while (x_index_float < 0)
		x_index_float += x_max_index;
	// doing the same for z
	float z_max_index = (float)(m_ZStride - 1);
	if (z_index_float >= z_max_index) {
		unsigned grids_to_offset = (unsigned)(z_index_float / z_max_index);
		z_index_float -= (float)(grids_to_offset * z_max_index);
	}
	while (z_index_float < 0)
		z_index_float += z_max_index;
	// casting to our index and getting our lerp parameters
	unsigned x_index = (unsigned)x_index_float;
	float xt = x_index_float - (float)x_index;
	unsigned z_index = (unsigned)z_index_float;
	float zt = z_index_float - (float)z_index;
	unsigned vertex_index = z_index * m_XStride + x_index;
	return MeshPosition(vertex_index, xt, zt);
}

Complex WaterFFT::HTilde(const Complex & htilde0,
	const Complex & htilde0_conjugate, const glm::vec2 & k, float time)
{
	// h~(k, t) = h~0(k) * exp(i * w(k) * t) + h~0*(-k) * exp(-i * w(k) * t)
	// h~   = htilde
	// h~0  = htilde0
	// w(k) = dispersion relation
	// All values of h~0 are precomputed in InitializeVertexBuffer
	float dispersion = DispersionRelation(k);
	float omega_t = dispersion * time;
	float cos_omega_t = cos(omega_t);
	float sin_omega_t = sin(omega_t);
	Complex e_1(cos_omega_t, sin_omega_t);
	Complex e_2(-cos_omega_t, -sin_omega_t);
	Complex term1 = htilde0 * e_1;
	Complex term2 = htilde0_conjugate * e_2;
	return term1 + term2;
}

float WaterFFT::DispersionRelation(const glm::vec2 & k)
{
	float w_0 = TAU / 200.0f;
	float k_magnitude = glm::length(k);
	return floor(sqrt(m_Gravity * k_magnitude) / w_0) * w_0;
}

Complex WaterFFT::HTilde0(const glm::vec2 & k)
{
	// h~0(k) = (q0 + i * q1) * sqrt(P(k) / 2)
	// h~0    = htilde0
	// q0, q1 = values from gaussian number generator
	// P(k)   = phillips spectrum
	float multiplicand = sqrt(PhillipsSpectrum(k) / 2.0f);
	Complex multiplier = NormalComplexRandom();
	Complex product = multiplier * multiplicand;
	return product;
}

float WaterFFT::PhillipsSpectrum(const glm::vec2 & k)
{
	float k_magnitude = glm::length(k);
	float wind_speed = glm::length(m_Wind);
	glm::vec2 wind_normal = m_Wind / wind_speed;
	if (k_magnitude < EPSILON) return 0.0f;
	float k_mag_pow_2 = k_magnitude * k_magnitude;
	float k_mag_pow_4 = k_mag_pow_2 * k_mag_pow_2;
	float largest_wave = wind_speed * wind_speed / m_Gravity;
	float largest_wave_pow_2 = largest_wave * largest_wave;
	glm::vec2 k_normal = k / k_magnitude;
	float k_dot_winddir = glm::dot(k_normal, wind_normal);
	float k_dot_winddir_pow_2 = k_dot_winddir * k_dot_winddir;
	float exponent = -1.0f / (k_mag_pow_2 * largest_wave_pow_2);
	// taken from keiths article.
	// I understand why this damping factor is here,
	// but I don't understand WHY it is here
	// Is there something we can do earlier to eliminate it
	float damping = 0.001f;
	float l2 = largest_wave_pow_2 * damping * damping;
	float factor2 = exp(exponent) / k_mag_pow_4;
	// TODO: Mess around with this spectrum, Keith's implementation has
	// a multiple factor that is attatched to this that might have a dramatic
	// effect. It is also mentioned in the tessendorf article
	// The multiplicative factor is below
	float additional_factor = exp(-k_mag_pow_2 * l2);
	return m_Amplitude * factor2 * k_dot_winddir_pow_2 * additional_factor;
}

inline void WaterFFT::InitializeVertexBuffer()
{
	// Clear the vertex data if it happens to exist.
	m_VertexBufferA.clear();

	// Set the starting positions of the vertices and calculate the htilde vertex
	// extra values that will be used for the fft computation.
	m_VertexBufferA.reserve(m_NumVerts);
	for (unsigned z = 0; z < m_ZStride; ++z)
	{
		float m = z - (m_fft_ZStride / 2.0f);
		float kz = (TAU * m) / m_ZLength; // TODO: Distribute the 2

		for (unsigned x = 0; x < m_XStride; ++x)
		{
			float n = x - (m_fft_XStride / 2.0f);

			// Find the vertice's starting position values.
			float start_x = m_XLength * n / m_fft_XStride;
			float start_y = 0.0f;
			float start_z = m_ZLength * m / m_fft_ZStride;

			// Add the new vertex to the vertex buffers.
			m_VertexBufferA.push_back(
				Vertex(start_x, start_y, start_z, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f));
			m_VertexBufferB.push_back(
				Vertex(start_x, start_y, start_z, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f));

			// Add the extra htilde values to the vertex extras buffer.
			if (z < m_fft_ZStride && x < m_fft_XStride)
			{
				float kx = (TAU * n) / m_XLength; // TODO: Distribute the 2
				glm::vec2 k(kx, kz);
				Complex htilde0_vertex = HTilde0(k);
				Complex htilde0_conjugate_vertex = HTilde0(-k).Conjugate();
				m_VertexExtrasBuffer.push_back(
					VertexExtra(start_x, start_y, start_z,
						htilde0_vertex, htilde0_conjugate_vertex));
			}
		}
	}

	// Vertex buffer A will be used to the first frame of the water. The
	// simulation will being writing the next frame to vertex buffer B.
	m_ReadBuffer = &m_VertexBufferA;
	m_WriteBuffer = &m_VertexBufferB;
}

inline void WaterFFT::InitializeIndexBuffer()
{
	m_IndexBuffer.clear();
	unsigned index_buffer_size = INDICIES_PER_QUAD * (m_NumVerts - m_XStride);
	m_IndexBuffer.reserve(index_buffer_size);
	// Finding indicies
	unsigned limit = m_NumVerts - m_XStride;
	// One iteration for each quad on the surface
	for (unsigned int i = 0; i < limit;) {
		m_IndexBuffer.push_back(i);
		m_IndexBuffer.push_back(i + 1);
		m_IndexBuffer.push_back(i + m_XStride);
		// First half of the quad
		// Triangle indices are i, i + 1, and i + m_XStride
		++i;
		m_IndexBuffer.push_back(i);
		m_IndexBuffer.push_back(i + m_XStride);
		m_IndexBuffer.push_back(i + m_XStride - 1);
		// Second half of the quad
		// Triangle indicies are i, i + m_XStride, and i + m_XStride - 1
		unsigned vertices_left = (i + 1) % m_XStride;
		if (vertices_left == 0)
			++i;
	}
}

inline void WaterFFT::InitializeOffsetBuffer(unsigned expansion)
{
	int num_instances = expansion * expansion;
	for (int i = 0; i < num_instances; ++i)
	{
		int x = i % expansion;
		int z = i / expansion;
		float x_offset = (float)x * m_XLength;
		float z_offset = (float)z * m_ZLength;
		m_OffsetBuffer.push_back(Offset(x_offset, 0.0f, z_offset, 1.0f));
	}
}

// WATERFFTERROR

WaterFFTError::Type WaterFFTError::GetType() const
{
	return m_Type;
}

const std::string & WaterFFTError::GetDescription() const
{
	return m_Description;
}

WaterFFTError::WaterFFTError(Type type, const std::string & description) :
	m_Type(type), m_Description(description)
{}