#pragma once
#include "pch.h"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include "Complex.h"

typedef unsigned int uint;
typedef unsigned char uchar;

class WaterFFT {
private:

	struct Vertex{
		Vertex(float x, float y, float z, float w,
			float nx, float ny, float nz, float nw) :
			m_Px(x), m_Py(y), m_Pz(z), m_Pw(w),
			m_Nx(nx), m_Ny(ny), m_Nz(nz), m_Nw(nw)
		{}
		float m_Px, m_Py, m_Pz, m_Pw;
		float m_Nx, m_Ny, m_Nz, m_Nw;
	};

	struct VertexExtra{
		VertexExtra(float ox, float oy, float oz, const Complex & htilde0,
			const Complex & htilde0_conjugate) : m_Ox(ox), m_Oy(oy), m_Oz(oz),
			m_HTilde0(htilde0), m_HTilde0Conjugate(htilde0_conjugate) {}
		//! Original vertex position.
		float m_Ox, m_Oy, m_Oz;
		//! Complex HTilde0(k) value for a vertex.
		Complex m_HTilde0;
		//! Complex HTilde0(-k) conjugate value for a vertex.
		Complex m_HTilde0Conjugate;
	};

	struct MeshPosition{
		MeshPosition(unsigned vertex_index, float xt, float zt) :
			m_VertexIndex(vertex_index), m_Xt(xt), m_Zt(zt) {}
		//! The vertex index on the mesh.
		unsigned m_VertexIndex;
		//! The linear interpolation parameter on the positive x axis.
		float m_Xt;
		//! The linear interpolation parameter on the positive z axis.
		float m_Zt;
	};

	struct IntensityMap{
		IntensityMap(const std::string & filename);
		~IntensityMap();
		float GetIntensity(float x, float z);
		std::string m_InstensityFile;
		uchar * m_Data;
		int m_Width;
		int m_Height;
		uint m_MaxX;
		uint m_MaxY;
		int m_Channels;
	};

	struct Offset{
		Offset(float x, float y, float z, float w) :
			m_Ox(x), m_Oy(y), m_Oz(z), m_Ow(w)
		{}
		float m_Ox, m_Oy, m_Oz, m_Ow;
	};

public:
	WaterFFT(unsigned grid_dimension, float meter_dimension, unsigned expansion,
		bool use_fft);
	~WaterFFT();
	bool UseIntensityMap(const std::string & filename);
	bool RemoveIntensityMap();
	std::pair<float, glm::vec3> HeightNormalAtLocation(
		const glm::vec2 & location, float time);
	float HeightAtLocation(const glm::vec2 & location);
	void Update(float time);
	void SwapBuffers();
	const void * VertexBuffer();
	const void * IndexBuffer();
	const void * OffsetBuffer();
	unsigned VertexBufferSizeBytes();
	unsigned IndexBufferSizeBytes();
	unsigned IndexBufferSize();
	unsigned OffsetBufferSizeBytes();
	unsigned OffsetBufferSize();
	// Scaler for the height of verts
	float m_HeightScale;
	// Scaler for the displace of verts
	float m_DisplaceScale;

private:
	void UpdateFFT(float time);

	void UpdateTailEdge(char edge);

	std::pair<float, glm::vec3> GetLocationHeightNormalFFT(
		const glm::vec2 & location);

	float GetLocationHeightFFT(const MeshPosition & mesh_position);

	glm::vec3 GetLocationNormalFFT(const MeshPosition & mesh_position);

	MeshPosition LocationToMeshPosition(glm::vec2 location);

	Complex HTilde(const Complex & htilde0,
		const Complex & htilde0_conjugate, const glm::vec2 & k, float time);

	float DispersionRelation(const glm::vec2 & k);

	Complex HTilde0(const glm::vec2 & k);

	float PhillipsSpectrum(const glm::vec2 & k);

	void InitializeVertexBuffer();

	void InitializeIndexBuffer();

	void InitializeOffsetBuffer(unsigned expansion);

	// Vertex information for the complete mesh.
	//! Number of vertices on the x axis.
	unsigned m_XStride;
	//! Number of vertices on the z axis.
	unsigned m_ZStride;
	//! The total number of verts on the mesh.
	unsigned m_NumVerts;

	// Vertex information for the fft computation.
	//! Number of vertices on the x axis.
	unsigned m_fft_XStride;
	//! Number of vertices on the z axis.
	unsigned m_fft_ZStride;
	//! The total number of verts on the mesh.
	unsigned m_fft_NumVerts;

	//! The front and back buffers. m_ReadBuffer is the buffer that is currently
	// being treated as the front buffer.
	std::vector<Vertex> m_VertexBufferA;
	std::vector<Vertex> m_VertexBufferB;
	std::vector<Vertex> * m_ReadBuffer;
	std::vector<Vertex> * m_WriteBuffer;
	//! The index buffer used for rendering the mesh.
	std::vector<unsigned int> m_IndexBuffer;
	//! The positional offsets for each instance. The size of this is number
	// of instances to be drawn.
	std::vector<Offset> m_OffsetBuffer;
	// Buffer for all of the extra vertex information (not needed for rendering)
	std::vector<VertexExtra> m_VertexExtrasBuffer;
	// Used for computing FFT
	// Input arrays
	Complex * m_HTildeIn;
	Complex * m_HTildeSlopeXIn;
	Complex * m_HTildeSlopeZIn;
	Complex * m_HTildeDisplaceXIn;
	Complex * m_HTildeDisplaceZIn;
	// Ouput arrays
	Complex * m_HTildeOut;
	Complex * m_HTildeSlopeXOut;
	Complex * m_HTildeSlopeZOut;
	Complex * m_HTildeDisplaceXOut;
	Complex * m_HTildeDisplaceZOut;
	// The plans for computing FFT.
	fftwf_plan m_HTildeFFTWPlan;
	fftwf_plan m_HTildeSlopeXPlan;
	fftwf_plan m_HTildeSlopeZPlan;
	fftwf_plan m_HTildeDisplaceXPlan;
	fftwf_plan m_HTildeDisplaceZPlan;
	//! The intensity map used for scaling sections of the simulation.
	IntensityMap * m_IMap;
	//! The length of the mesh in the x direction in meters.
	float m_XLength;
	//! The length of the mesh in the z direction in meters.
	float m_ZLength;
	//! This will affect amplitude, but it is not directly
	// representitive of wave heights.
	float m_Amplitude;
	//! The gravitional constant.
	float m_Gravity;
	//! The wind direction and magnitude.
	glm::vec2 m_Wind;
};

class WaterFFTError
{
public:
	//! The types of WaterFFT errors that can occur.
	enum Type
	{
		INVALID_GRID_DIM,
		SMALL_DX_DZ
	};
	Type GetType() const;
	const std::string & GetDescription() const;
private:
	WaterFFTError(Type type, const std::string & description);
	//! The type of the error.
	Type m_Type;
	//! A description of the error.
	std::string m_Description;
	friend WaterFFT;
};