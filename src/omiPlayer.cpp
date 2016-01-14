#include "stdafx.h"
#include "RGB_PixelShader.h"
#include "RGB_VertexShader.h"
#include "YUV_PixelShader.h"
#include "YUV_VertexShader.h"
#include "VideoPlayer.h"
#include "Console.h"
#include "resource.h"
#include "trackCamera.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb.h"
#include "stb_image.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#ifdef _MSC_VER
#include <boost/config/compiler/visualc.hpp>
#endif
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/filesystem.hpp>

using namespace DirectX;

bool MIRROR = true;

// Windows stuff
HANDLE hSwapchainWait{ NULL };
HWND hWnd{ NULL };
RECT windowClientRect{ 0, 0, 1920, 1080 };

// UCL CAVE spec. 
// RECT caveWindowClientRect{ 0, 0, 5300, 1050 };
// this is 1080p but probably needs to be changed for dvi screen
float caveScreenHeight = 1050.0f;
float insetScreenHeight = 1080.0f;
float insetScreenWidth = 1920.0f;
float insetZoom = 0.1f;
float caveForwardDegrees = 0.0f;
float caveUpDegrees = 0.0f;
RECT caveWindowClientRect{ 0, 0, (1400 * 3) + insetScreenWidth, (insetScreenHeight > caveScreenHeight) ? insetScreenHeight : caveScreenHeight };

char windowTitle[]{"omiPlayer for Oculus by @omigamedev - DX11"};
bool active{ true };
bool playing{ false };
bool first_frame{ true };

// ME
float cameraYaw{ 0.0f };
float cameraPitch{ 0.0f };
float insetYaw{ 0.0f };

float cameraZ{ 0.0f };
float cameraX{ 0.0f };
float cameraY{ 0.0f };
float cameraW{ 0.0f };

float insetPitch{ 0.0f };

// config settings for omni (multi) player
bool shouldSendUDPPackets { false };
bool shouldCameraTrack{ false };
std::string insetFileName{ "" };
std::string udp_ip_address{ "" };
std::string cameraTrackCSV{ "" };
int udp_port{ 0 };
float initialX{ 0.0f };
float initialY{ 0.0f };
float initialZ{ 0.0f };

bool lockToYRotation{ false };

// Source
enum class MediaType
{
	IMAGE,
	VIDEO
};
MediaType media;
bool stereo = true;
int texWidth = 0;
int texHeight = 0;


// Media stuff
VideoPlayer player;
bool bottomUp = true;
int frameIndex = 0;
uint8_t* frameMappedData = nullptr;
float frameTime = 0.3f;
std::atomic<bool> frameReady = false;

// Audio stuff
ALCdevice*  oalDevice  = nullptr;
ALCcontext* oalContext = nullptr;

// Oculus stuff
ovrHmd hmd;
ovrFrameTiming timer;
ovrEyeRenderDesc EyeRenderDesc[2]; // Description of the VR.
ovrRecti EyeRenderViewport[2];     // Useful to remember when varying resolution
ovrPosef EyeRenderPose[2];         // Useful to remember where the rendered eye originated
OVR::Sizei idealSize;

//CAVE
bool caveMode = true;
D3D11_VIEWPORT caveViewports[4];


ID3D11Device*             d3d_device       { nullptr };
ID3D11DeviceContext*      d3d_context      { nullptr };
IDXGISwapChain*           dx_swapchain     { nullptr };
IDXGISwapChain2*          dx_swapchain2    { nullptr };
ID3D11RenderTargetView*   d3d_backbuffer   { nullptr };
ID3D11DepthStencilView*   d3d_depth        { nullptr };
ID3D11DepthStencilState*  d3d_depthState   { nullptr };
ID3D11RasterizerState*    d3d_rasterizer   { nullptr };
// Shader data            
ID3D11PixelShader*        d3d_pixelShader  { nullptr };
ID3D11VertexShader*       d3d_vertexShader { nullptr };
// Vertex buffer data     
ID3D11InputLayout*        d3d_inputLayout  { nullptr };
// Matrix                 
ID3D11Buffer*             d3d_perFrameBuffer { nullptr };
ID3D11Buffer*             d3d_perAppBuffer   { nullptr };
// Mesh data              
ID3D11Buffer*             d3d_sphereVertexBuffer { nullptr };
ID3D11Buffer*             d3d_sphereIndexBuffer  { nullptr };
ID3D11Buffer*             d3d_rectVertexBuffer   { nullptr };
ID3D11Buffer*             d3d_rectIndexBuffer    { nullptr };
ID3D11InputLayout*        d3d_vertexLayout       { nullptr };
// Texture data           
ID3D11SamplerState*       d3d_sampler        { nullptr };
ID3D11Texture2D*          d3d_texture[2]     { nullptr };
ID3D11ShaderResourceView* d3d_textureView[2] { nullptr };
// Render to texture
ID3D11RenderTargetView*   d3d_rttRenderView[2]  { nullptr };
ID3D11ShaderResourceView* d3d_rttTextureView[2] { nullptr };
ID3D11Texture2D*          d3d_rttTexture[2]     { nullptr };

// Vertex data for a colored cube.
__declspec(align(16)) struct Vertex
{
	XMFLOAT3 Position;
	XMFLOAT2 Texcoord;
	Vertex() = default;
	Vertex(XMFLOAT3 p, XMFLOAT2 t) : Position(p), Texcoord(t) { }
};

__declspec(align(16)) struct PerFrameBuffer
{
	XMMATRIX modelview;
	XMMATRIX texmat;
	PerFrameBuffer() = default;
	PerFrameBuffer(const XMMATRIX& modelview, const XMMATRIX& texmat) 
		: modelview(modelview), texmat(texmat) {}
};

__declspec(align(16)) struct PerAppBuffer
{
	XMMATRIX proj;
	PerAppBuffer() = default;
	PerAppBuffer(const XMMATRIX& proj) : proj(proj) {}
};

std::vector<Vertex> sphereVertices;
std::vector<WORD> sphereIndices;

std::vector<Vertex> rectVertices
{
	{ XMFLOAT3(-1,-1,0), XMFLOAT2(0,1) }, // 0
	{ XMFLOAT3(-1, 1,0), XMFLOAT2(0,0) }, // 1
	{ XMFLOAT3( 1, 1,0), XMFLOAT2(1,0) }, // 2
	{ XMFLOAT3( 1,-1,0), XMFLOAT2(1,1) }, // 3
};
std::vector<WORD> rectIndices{ 0, 1, 2, 3, 0, 2 };

// Mouse control
XMFLOAT2 dragStart, dragDiff{ 0.f, 0.f }, dragCamera, cameraRotation;
bool drag{ false };

D3D11_INPUT_ELEMENT_DESC vertexLayoutDesc[]
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT   , 0, offsetof(Vertex, Texcoord), D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

void InitCAVE()
{
	for (int i = 0; i < 4; i++)
	{
		//Generic CAVE
//		caveViewports[i].Width = (float)windowClientRect.right / 4;
//		caveViewports[i].TopLeftX = i* (float)windowClientRect.right / 4;
		// UCL CAVE
		switch (i)
		{
		case 0:
			caveViewports[i].Width = 1400;
			caveViewports[i].TopLeftX = 0;
			caveViewports[i].Height = 1050.0f;
			break;
		case 1:
			caveViewports[i].Width = 1400;
			caveViewports[i].TopLeftX = 1400;
			caveViewports[i].Height = 1050.0f;
			break;
		case 2:
			caveViewports[i].Width = 1400;
			caveViewports[i].TopLeftX = 2800;
			caveViewports[i].Height = 1050.0f;
			break;
		case 3:
		default:
			//caveViewports[i].Width = 1100;
			caveViewports[i].TopLeftX = 4200;
			//caveViewports[i].Height = (float)caveWindowClientRect.bottom;
			caveViewports[i].Height = insetScreenHeight;
			caveViewports[i].Width = insetScreenWidth; // caveWindowClientRect.bottom;
			break;
		}

		// caveViewports[i].Height = (float)caveWindowClientRect.bottom;
		caveViewports[i].TopLeftY = 0.0f;
		caveViewports[i].MinDepth = 0.0f;
		caveViewports[i].MaxDepth = 1.0f;
	}
}

void InitD3D()
{
	UINT stride = sizeof(Vertex), offset = 0;

	d3d_context->IASetInputLayout(d3d_vertexLayout);
	d3d_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	d3d_context->RSSetState(d3d_rasterizer);

	d3d_context->PSSetShader(d3d_pixelShader, nullptr, 0);
	d3d_context->PSSetSamplers(0, 1, &d3d_sampler);
	d3d_context->PSSetShaderResources(0, 1, &d3d_textureView[frameIndex]);

	d3d_context->VSSetShader(d3d_vertexShader, nullptr, 0);
	d3d_context->VSSetConstantBuffers(0, 1, &d3d_perAppBuffer);
	d3d_context->VSSetConstantBuffers(1, 1, &d3d_perFrameBuffer);

	d3d_context->IASetIndexBuffer(d3d_sphereIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	d3d_context->IASetVertexBuffers(0, 1, &d3d_sphereVertexBuffer, &stride, &offset);
}

void RenderFrame(int eyeID)
{
	D3D11_VIEWPORT vp_rtt{ 0.f, 0.f, (float)idealSize.w, (float)idealSize.h, 0.f, 1.f };

	ovrEyeType eye = hmd->EyeRenderOrder[eyeID];
	ovrFovPort fov = hmd->DefaultEyeFov[eyeID];
	ovrEyeRenderDesc eyeDesc = ovrHmd_GetRenderDesc(hmd, ovrEyeType::ovrEye_Left, fov);

	ovrPosef pose = ovrHmd_GetHmdPosePerEye(hmd, eye);

	// Get view and projection matrices
	OVR::Matrix4f proj = ovrMatrix4f_Projection(eyeDesc.Fov, 0.0f, 1000.0f, true);
	OVR::Matrix4f orient = OVR::Matrix4f(pose.Orientation);
	
	// Transform to OpenGL compatible matrix
	XMMATRIX eyeProj(&proj.Transposed().M[0][0]);
	XMMATRIX modelview(&orient.M[0][0]);
	XMMATRIX texmat = XMMatrixIdentity();

	if (bottomUp)
		texmat = XMMatrixScaling(1, .5f, 1) * XMMatrixTranslation(0, (eyeID ? 0 : 0.5f), 0);
	
	PerAppBuffer perApp(eyeProj);
	PerFrameBuffer perFrame(XMMatrixRotationY(XMConvertToRadians(-90)) * modelview, texmat);

	// Render to texture
	d3d_context->UpdateSubresource(d3d_perFrameBuffer, 0, nullptr, &perFrame, 0, 0);
	d3d_context->UpdateSubresource(d3d_perAppBuffer, 0, nullptr, &perApp, 0, 0);
	d3d_context->RSSetViewports(1, &vp_rtt);
	d3d_context->OMSetRenderTargets(1, &d3d_rttRenderView[eye], nullptr);
	//d3d_context->ClearRenderTargetView(d3d_rttRenderView[eye], DirectX::Colors::White);
	d3d_context->PSSetShaderResources(0, 1, &d3d_textureView[frameIndex]);

	d3d_context->DrawIndexed(sphereIndices.size(), 0, 0);
}

void RenderCAVEFrame(int wallID)
{
	XMMATRIX eyeProj;

	XMMATRIX modelview;

	// Dependent on the specific CAVE configuration. Assumes COP is centre of CAVE. Adjust to suit and debate
	// whether head tracking is worth it or not. We needed to support multiple people in CAVE.

	switch (wallID)
	{
	case 0:
		eyeProj = XMMatrixPerspectiveRH(0.2f, 0.2f * 1050.0f / 1400.0f, 0.1f, 1000.0f);
		modelview = XMMatrixRotationY(XMConvertToRadians(-180));
		//eyeProj = XMMatrixPerspectiveRH(0.1f, 0.1f * 1050.0f / 1400.0f, 0.1f, 1000.0f);
		//modelview = XMMatrixRotationY(XMConvertToRadians(-90));
		break;
	case 1:
		eyeProj = XMMatrixPerspectiveRH(0.2f, 0.2f * 1050.0f / 1400.0f, 0.1f, 1000.0f);
		modelview = XMMatrixRotationY(XMConvertToRadians(-90));
		break;
	case 2:
		eyeProj = XMMatrixPerspectiveRH(0.2f, 0.2f * 1050.0f / 1400.0f, 0.1f, 1000.0f);
		modelview = XMMatrixIdentity();
		break;
	case 3:
		/*
		// 1033 & 1125 found by experimentation because the UCL CAVE has an odd asymmetrical projection on floor
		eyeProj = XMMatrixPerspectiveOffCenterRH(-0.1f * 1400.0f / 1050.0f, 0.1f * 1400.0f / 1050.0f,
		-0.1f * 1400.0f / 1033.0f,
		0.1f * 1400.0f / 1125.0f,
		0.1f, 1000.0f);
		modelview = XMMatrixRotationY(XMConvertToRadians(90)) * modelview;*/
		eyeProj = XMMatrixPerspectiveRH(insetZoom, insetZoom * insetScreenHeight / insetScreenWidth, 0.1f, 1000.0f);
		modelview = XMMatrixRotationY(XMConvertToRadians(-90));
		modelview = modelview * XMMatrixRotationRollPitchYaw(XMConvertToRadians(insetPitch), XMConvertToRadians(insetYaw), 0.0f);
		break;
	}

	//XMMATRIX cameraTrackMatrix = XMMatrixRotationRollPitchYaw(XMConvertToRadians(cameraPitch), XMConvertToRadians(cameraYaw), 0.0f);

	XMMATRIX initialMatrix = XMMatrixRotationZ(initialZ) * XMMatrixRotationX(initialX) * XMMatrixRotationY(initialY);

	// XMMATRIX cameraTrackMatrix = XMMatrixRotationZ(cameraZ) * XMMatrixRotationX(cameraX) * XMMatrixRotationY(cameraY);
	
	XMMATRIX cameraTrackMatrix = XMMatrixRotationQuaternion(XMVectorSet(cameraX, cameraY, cameraZ, cameraW));

	// rhs to lhs, from http://answers.unity3d.com/storage/temp/12048-lefthandedtorighthanded.pdf
	// also this: http://www.gamedev.net/topic/385001-converting-a-transformation-matrix-from-right-handed-to-left-handed-coord-system/
	XMMATRIX flipzMatrix = XMMATRIX(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	cameraTrackMatrix = flipzMatrix * cameraTrackMatrix * flipzMatrix;

	//if(lockToYRotation)
	//	cameraTrackMatrix = XMMatrixRotationQuaternion(XMVectorSet(0.0f, cameraY, 0.0f, cameraW * -1.0f));
	
	//cameraTrackMatrix = XMMatrixRotationY(XMConvertToRadians(-90)) * cameraTrackMatrix * XMMatrixRotationY(XMConvertToRadians(90));
	
	//eyeProj = initialMatrix * cameraTrackMatrix * eyeProj;
	//modelview = cameraTrackMatrix * XMMatrixRotationY(XMConvertToRadians(caveForwardDegrees)) * XMMatrixRotationZ(XMConvertToRadians(caveUpDegrees)) * modelview;

	//modelview = cameraTrackMatrix * modelview;
	cameraTrackMatrix = XMMatrixRotationQuaternion(XMVectorSet(cameraX, cameraY, cameraZ, cameraW * -1.0f));

	cameraTrackMatrix = XMMatrixRotationY(XMConvertToRadians(-90)) * cameraTrackMatrix * XMMatrixRotationY(XMConvertToRadians(90));

	modelview = cameraTrackMatrix * modelview;
	eyeProj = eyeProj;




	XMMATRIX texmat = XMMatrixIdentity();
	if (bottomUp)
		texmat = XMMatrixScaling(1, .5f, 1);

	PerAppBuffer perApp(eyeProj);

	PerFrameBuffer perFrame(modelview, texmat);

	d3d_context->RSSetViewports(1, &caveViewports[wallID]);

	d3d_context->UpdateSubresource(d3d_perFrameBuffer, 0, nullptr, &perFrame, 0, 0);
	d3d_context->UpdateSubresource(d3d_perAppBuffer, 0, nullptr, &perApp, 0, 0);

	d3d_context->DrawIndexed(sphereIndices.size(), 0, 0);
}

void UpdateVideo()
{
	if (media == MediaType::VIDEO && frameReady)
	{
		d3d_context->Unmap(d3d_texture[1 - frameIndex], 0);

		frameIndex = 1 - frameIndex;

		// Map the frame to be filled by the decoder
		D3D11_MAPPED_SUBRESOURCE texmap;
		HRESULT res = d3d_context->Map(d3d_texture[1 - frameIndex], 0, D3D11_MAP_WRITE_DISCARD, 0, &texmap);
		assert(res == S_OK);
		frameMappedData = (uint8_t*)texmap.pData;

		frameReady = false;
	}
}

std::istream& operator>>(std::istream& str, TrackCamera& data)
{
	data.readNextRow(str);
	return str;
}

void Render(float dt)
{
	static float elapsed = 0;
	elapsed += dt;
	if (hmd)
	{
		ovrD3D11Texture eyeTexture[2];
		for (int eye = 0; eye < 2; eye++)
		{
			EyeRenderViewport[eye].Pos = OVR::Vector2i(0, 0);
			EyeRenderViewport[eye].Size = idealSize;

			eyeTexture[eye].D3D11.Header.API = ovrRenderAPI_D3D11;
			eyeTexture[eye].D3D11.Header.TextureSize = idealSize;
			eyeTexture[eye].D3D11.Header.RenderViewport = EyeRenderViewport[eye];
			eyeTexture[eye].D3D11.pTexture = d3d_rttTexture[eye];
			eyeTexture[eye].D3D11.pSRView = d3d_rttTextureView[eye];
		}

		//DWORD result = WaitForSingleObjectEx(
		//    hSwapchainWait,
		//    1000, // 1 second timeout (shouldn't ever occur)
		//    true
		//    );


		ovrHmd_BeginFrame(hmd, 0);

		RenderFrame(0);
		RenderFrame(1);

		UpdateVideo();

		ovrVector3f useHmdToEyeViewOffset[2] = { EyeRenderDesc[0].HmdToEyeViewOffset, EyeRenderDesc[1].HmdToEyeViewOffset };
		ovrHmd_GetEyePoses(hmd, 0, useHmdToEyeViewOffset, EyeRenderPose, NULL);
		ovrHmd_EndFrame(hmd, EyeRenderPose, &eyeTexture[0].Texture);
	}
	else if (caveMode)
	{
		XMMATRIX modelView;
		UINT stride = sizeof(Vertex), offset = 0;

		d3d_context->OMSetRenderTargets(1, &d3d_backbuffer, nullptr);
		d3d_context->ClearRenderTargetView(d3d_backbuffer, DirectX::Colors::Gray);
		d3d_context->ClearDepthStencilView(d3d_depth, D3D11_CLEAR_DEPTH, 1, 0);
		d3d_context->PSSetShaderResources(0, 1, &d3d_textureView[frameIndex]);

		for (int i = 0; i < 4; i++)
		{
			RenderCAVEFrame(i);
		}

		UpdateVideo();

		dx_swapchain->Present(1, 0);
	}
	else
	{
		XMMATRIX modelView;
		UINT stride = sizeof(Vertex), offset = 0;

		D3D11_VIEWPORT vp_screen{ 0.f, 0.f, (float)windowClientRect.right, (float)windowClientRect.bottom, 0.f, 1.f };
//		D3D11_VIEWPORT vp_rtt{ 0.f, 0.f, (float)idealSize.w, (float)idealSize.h, 0.f, 1.f };

		PerFrameBuffer perFrame;
		perFrame.texmat = XMMatrixIdentity();

		if (bottomUp)
			perFrame.texmat = XMMatrixScaling(1, .5f, 1);

		// Render to the screen
		perFrame.modelview = XMMatrixTranslation(0, 0, 1);// *XMMatrixScaling(.7f, .7f, .7f);
		d3d_context->UpdateSubresource(d3d_perFrameBuffer, 0, nullptr, &perFrame, 0, 0);
		d3d_context->OMSetRenderTargets(1, &d3d_backbuffer, nullptr);
		d3d_context->ClearRenderTargetView(d3d_backbuffer, DirectX::Colors::Gray);
		d3d_context->ClearDepthStencilView(d3d_depth, D3D11_CLEAR_DEPTH, 1, 0);
		//d3d_context->PSSetShaderResources(0, 1, &d3d_rttTextureView[0]);
		d3d_context->PSSetShaderResources(0, 1, &d3d_textureView[frameIndex]);
		d3d_context->IASetIndexBuffer(d3d_rectIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
		d3d_context->IASetVertexBuffers(0, 1, &d3d_rectVertexBuffer, &stride, &offset);
		d3d_context->RSSetViewports(1, &vp_screen);
		d3d_context->DrawIndexed(rectIndices.size(), 0, 0);

		UpdateVideo();
		dx_swapchain->Present(1, 0);
	}
	elapsed -= frameTime;

}

bool OpenAL_Init()
{
	oalDevice = alcOpenDevice(nullptr);
	if (!oalDevice)
		return false;
	oalContext = alcCreateContext(oalDevice, nullptr);
	if (!oalContext)
		return false;
	alcMakeContextCurrent(oalContext);
	return true;
}

void OpenAL_Destroy()
{
	alcMakeContextCurrent(nullptr);
	alcDestroyContext(oalContext);
	alcCloseDevice(oalDevice);
}

void SendDataPacket(string message) {
	SOCKET sock;
	addrinfo* pAddr;
	addrinfo hints;
	sockaddr sAddr;

	WSADATA wsa;
	unsigned short usWSAVersion = MAKEWORD(2, 2);

	char * Buffer = new char[message.length() + 1];
	std::strcpy(Buffer, message.c_str());

	int ret;

	//Start WSA
	WSAStartup(usWSAVersion, &wsa);

	//Create Socket
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	//Resolve host address
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_socktype = SOCK_DGRAM;

	std::string port = std::to_string(udp_port);
	if (getaddrinfo(udp_ip_address.c_str(), port.c_str() , &hints, &pAddr))
	{
		std::cerr << "Could not resolve address...\n";
		std::cin.get();
		WSACleanup();
		return;
	}

	//Start Transmission
	ret = sendto(sock, Buffer, message.length() + 1, 0, pAddr->ai_addr, pAddr->ai_addrlen);
	if (ret != sizeof(Buffer))
	{
		std::cerr << "Could not send data\n";
		std::cin.get();
		WSACleanup();
		return;
	}

	WSACleanup();
}

LRESULT CALLBACK MainProc(HWND hWnd, UINT msgID, WPARAM wp, LPARAM lp)
{
	static char title[256];
	switch (msgID)
	{
	case WM_CREATE:
		return 0;
	case WM_LBUTTONDOWN:
	{
		int x = GET_X_LPARAM(lp);
		int y = GET_Y_LPARAM(lp);
		drag = true;
		dragDiff = XMFLOAT2(.0f, .0f);
		dragStart = XMFLOAT2((float)x, (float)y);
		dragCamera = cameraRotation;
		SetCapture(hWnd);
		return 0;
	}
	case WM_MOUSEMOVE:
	{
		int x = GET_X_LPARAM(lp);
		int y = GET_Y_LPARAM(lp);
		dragDiff.x = dragStart.x - x;
		dragDiff.y = dragStart.y - y;
		if (drag)
		{
			cameraRotation.x = dragCamera.x + dragDiff.x;
			cameraRotation.y = dragCamera.y + dragDiff.y;
		}
		return 0;
	}
	case WM_LBUTTONUP:
	{
		drag = false;
		SetCapture(NULL);
		return 0;
	}
	case WM_KEYDOWN:
		if (wp == VK_SPACE)
		{
			playing = !playing;
			sprintf_s(title, "%s - %s", windowTitle, playing ? "Playing" : "Paused");
			printf("%s\n", title);
			if (!caveMode)
				SetWindowText(hWnd, title);

			if (shouldSendUDPPackets) {
				SendDataPacket(playing ? "PLAY" : "PAUSE");
			}
		}
		if (wp == VK_F1)
		{
			Console::RedirectIOToConsole();
		}
		if (wp == 73) // the I key (for zoom in)
		{
			if (insetZoom > 0.05)
				insetZoom = insetZoom - 0.01f;
		}
		if (wp == 79) // the o key (for zoom out)
		{
			insetZoom = insetZoom + 0.01f;
		}
		if (wp == 65) // the a key (rotate whole panorama anticlockwise)
		{
			caveForwardDegrees = caveForwardDegrees + 10;
		}
		if (wp == 67) // the c key (rotate whole panorama clockwise)
		{
			caveForwardDegrees = caveForwardDegrees - 10;
		}
		if (wp == 68) // the d key (rotate whole panorama downwards)
		{
			caveUpDegrees = caveUpDegrees + 2;
		}
		if (wp == 85) // the u key (rotate whole panorama upwards)
		{
			caveUpDegrees = caveUpDegrees - 2;
		}
		if (wp == 37) // left arrow
		{
			insetYaw = insetYaw - 0.1f;
		}
		if (wp == 38) // up arrow
		{
			insetPitch = insetPitch - 0.1f;
		}
		if (wp == 39) // right arrow
		{
			insetYaw = insetYaw + 0.1f;
		}
		if (wp == 40) // down arrow
		{
			insetPitch = insetPitch + 0.1f;
		}
		if (wp == VK_ESCAPE)
		{
			PostQuitMessage(0);
		}
		break;
	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, msgID, wp, lp);
}

void SphereCreate(float radius, int rings, int sectors)
{
	float phiStep = (float)M_PI / rings;
	float thetaStep = 2.0f * (float)M_PI / sectors;

	for (int i = 1; i <= rings - 1; i++)
	{
		float phi = i * phiStep;
		for (int j = 0; j <= sectors; j++) 
		{
			float theta = j * thetaStep;
			
			float x = radius * std::sinf(phi) * cosf(theta);
			float y = radius * std::cosf(phi);
			float z = radius * std::sinf(phi) * sinf(theta);
			float u = theta / ((float)M_PI * 2.0f);
			float v = phi / (float)M_PI;
			sphereVertices.emplace_back(XMFLOAT3(x, y, z), XMFLOAT2(u, v));
		}
	}

	for (int i = 1; i <= sectors; i++) 
	{
		sphereIndices.push_back(0);
		sphereIndices.push_back(i + 1);
		sphereIndices.push_back(i);
	}
	int baseIndex = 0;
	int ringVertexCount = sectors + 1;
	for (int i = 0; i < rings - 2; i++) 
	{
		for (int j = 0; j < sectors; j++) 
		{
			sphereIndices.push_back(baseIndex + i*ringVertexCount + j);
			sphereIndices.push_back(baseIndex + i*ringVertexCount + j + 1);
			sphereIndices.push_back(baseIndex + (i + 1)*ringVertexCount + j);

			sphereIndices.push_back(baseIndex + (i + 1)*ringVertexCount + j);
			sphereIndices.push_back(baseIndex + i*ringVertexCount + j + 1);
			sphereIndices.push_back(baseIndex + (i + 1)*ringVertexCount + j + 1);
		}
	}
	int southPoleIndex = sphereVertices.size() - 1;
	baseIndex = southPoleIndex - ringVertexCount - 1;
	for (int i = 0; i < sectors; i++) 
	{
		sphereIndices.push_back(southPoleIndex);
		sphereIndices.push_back(baseIndex + i);
		sphereIndices.push_back(baseIndex + i + 1);
	}

	HRESULT res;

	// Create vertex buffer
	{
		int size = sizeof(Vertex) * sphereVertices.size();
		// size, usage, bind, cpu_access, misc, stride
		D3D11_BUFFER_DESC desc{ size, D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
		D3D11_SUBRESOURCE_DATA data{ &sphereVertices[0], 0, 0 };
		res = d3d_device->CreateBuffer(&desc, &data, &d3d_sphereVertexBuffer);
		assert(res == S_OK);
	}

	// Create index buffer
	{
		int size = sizeof(WORD) * sphereIndices.size();
		// size, usage, bind, cpu_access, misc, stride
		D3D11_BUFFER_DESC desc{ size, D3D11_USAGE_DEFAULT, D3D11_BIND_INDEX_BUFFER, 0, 0, 0 };
		D3D11_SUBRESOURCE_DATA indexData{ &sphereIndices[0], 0, 0 };
		res = d3d_device->CreateBuffer(&desc, &indexData, &d3d_sphereIndexBuffer);
		assert(SUCCEEDED(res));
	}
}

void RectCreate()
{
	HRESULT res;
	// Create vertex buffer
	{
		int size = sizeof(Vertex) * rectVertices.size();
		// size, usage, bind, cpu_access, misc, stride
		D3D11_BUFFER_DESC desc{ size, D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0 };
		D3D11_SUBRESOURCE_DATA data{ &rectVertices[0], 0, 0 };
		res = d3d_device->CreateBuffer(&desc, &data, &d3d_rectVertexBuffer);
		assert(res == S_OK);
	}

	// Create index buffer
	{
		int size = sizeof(WORD) * rectIndices.size();
		// size, usage, bind, cpu_access, misc, stride
		D3D11_BUFFER_DESC desc{ size, D3D11_USAGE_DEFAULT, D3D11_BIND_INDEX_BUFFER, 0, 0, 0 };
		D3D11_SUBRESOURCE_DATA indexData{ &rectIndices[0], 0, 0 };
		res = d3d_device->CreateBuffer(&desc, &indexData, &d3d_rectIndexBuffer);
		assert(SUCCEEDED(res));
	}
}

void CreateRTT(int eye, int w, int h)
{
	HRESULT res;

	D3D11_TEXTURE2D_DESC texDesc{ 0 };
	texDesc.Width = w;
	texDesc.Height = h;
	texDesc.ArraySize = 1;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.MipLevels = 1;
	texDesc.MiscFlags = 0;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	res = d3d_device->CreateTexture2D(&texDesc, nullptr, &d3d_rttTexture[eye]);
	assert(res == S_OK);

	D3D11_RENDER_TARGET_VIEW_DESC renderDesc;
	ZeroMemory(&renderDesc, sizeof(renderDesc));
	renderDesc.Format = texDesc.Format;
	renderDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	res = d3d_device->CreateRenderTargetView(d3d_rttTexture[eye], &renderDesc, &d3d_rttRenderView[eye]);
	assert(res == S_OK);

	D3D11_SHADER_RESOURCE_VIEW_DESC resourceDesc;
	ZeroMemory(&resourceDesc, sizeof(resourceDesc));
	resourceDesc.Format = texDesc.Format;
	resourceDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	resourceDesc.Texture2D.MipLevels = 1;
	res = d3d_device->CreateShaderResourceView(d3d_rttTexture[eye], &resourceDesc, &d3d_rttTextureView[eye]);
	assert(res == S_OK);
}

void decodeThread()
{
	auto start = std::chrono::steady_clock::now();
	double elapsed{ 0 };

	D3D11_QUERY_DESC qdesc{ D3D11_QUERY_EVENT, 0 };
	ID3D11Query* d3d_query;
	HRESULT res = d3d_device->CreateQuery(&qdesc, &d3d_query);
	assert(SUCCEEDED(res));

	while (active)
	{
		auto stop = std::chrono::steady_clock::now();
		auto dt = std::chrono::duration<float>(stop - start).count();
		start = stop;

		if (playing)
			elapsed += dt;

		player.audio_play = playing;

		if (first_frame || (playing && elapsed > frameTime))
		{
			// drop frames
			while (elapsed > frameTime * 1.5f)
			{
				player.decodeFrame(nullptr);
				elapsed -= frameTime;
				//printf("frame discarded\n");
			}

			// wait GPU idle
			BOOL gpu_done = FALSE;
			while (!gpu_done && !FAILED(d3d_context->GetData(d3d_query, &gpu_done, sizeof(gpu_done), 0)))
			{
				printf("wait GPU\n");
			}
			
			if (!frameReady)
			{
				player.decodeFrame(nullptr, frameMappedData);
				frameReady = true;
				elapsed = 0;
			}

			first_frame = false;

		}
		//else
		//{
		//    int milli = (frameTime - elapsed) * 1000;
		//    std::this_thread::sleep_for(std::chrono::milliseconds(milli));
		//    printf("sleep %d milli\n", milli);
		//}

	}
}

void loadVideoConfig(boost::filesystem::path filename) {
	boost::filesystem::path configFile = (filename.parent_path() / boost::filesystem::path(filename.stem().string() + ".json"));

	try
	{
		std::ifstream file(configFile.string());
		if (file) {
			std::stringstream ss;
			ss << file.rdbuf();
			// send your JSON above to the parser below, but populate ss first
			boost::property_tree::ptree pt;
			boost::property_tree::read_json(ss, pt);
			file.close();

			if (boost::optional<std::string> ip_address = pt.get_optional<std::string>("ip_address")) {
				shouldSendUDPPackets = true;
				udp_ip_address = ip_address.value();
				udp_port = pt.get<int>("port");
			}

			if (boost::optional<std::string> tracking_csv = pt.get_optional<std::string>("tracking_csv")) {
				shouldCameraTrack = true;
				cameraTrackCSV = tracking_csv.value();
			}

			if (boost::optional<int> caveWidth = pt.get_optional<int>("cave_width")) {
				caveWindowClientRect.right = caveWidth.value();
			}

			if (boost::optional<int> caveHeightOverride = pt.get_optional<int>("cave_height_override")) {
				caveWindowClientRect.bottom = caveHeightOverride.value();
			}

			if (boost::optional<float> y = pt.get_optional<float>("initial_x")) {
				initialX = y.value();
				initialY = pt.get<float>("initial_y");
				initialZ = pt.get<float>("initial_z");
			}

			if (boost::optional<std::string> inset_file = pt.get_optional<std::string>("inset_file")) {
				if (shouldSendUDPPackets) {
					SendDataPacket("OPEN " + inset_file.value());
				}
			}

			if (boost::optional<bool> lockToY = pt.get_optional<bool>("lock_to_y_rotation")) {
				lockToYRotation = lockToY.value();
			}
		}
		else {
			// set defaults
		}

	}
	catch (std::exception const& e)
	{
		std::cerr << e.what() << std::endl;
	}


}

// Windows main entry point wrapper
int main(int argc, char* argv[]);
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
	LPWSTR *szArgList{ nullptr };
	int argc{ 0 };
	char** argv{ nullptr };

	szArgList = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (szArgList == NULL)
	{
		MessageBox(NULL, "Unable to parse command line", "Error", MB_OK);
		return 10;
	}

	argv = new char*[argc + 1];
	for (int i = 0; i < argc; i++)
	{
		auto len = wcslen(szArgList[i]) + 1;
		argv[i] = new char[len];
		wcstombs(argv[i], szArgList[i], len);
	}

	LocalFree(szArgList);

	return main(argc, argv);
}

int main(int argc, char* argv[])
{
	boost::filesystem::path p(argv[1]);
	loadVideoConfig(p);


	static char title[256];

	std::thread pollThread;
	HRESULT res;
	WNDCLASS wc{ 0 };
	wc.hInstance = GetModuleHandle(nullptr);
	wc.lpfnWndProc = MainProc;
	wc.lpszClassName = "MainDXWindowClass";
	wc.style = CS_HREDRAW | CS_VREDRAW;
	RegisterClass(&wc);

	if (!caveMode)
	{
		sprintf_s(title, "%s - Press spacebar to play", windowTitle);
		printf("%s\n", title);

		RECT adjustedRect{ 0 };
		adjustedRect.bottom = windowClientRect.bottom / 2;
		adjustedRect.right = windowClientRect.right / 2;
		AdjustWindowRect(&adjustedRect, WS_OVERLAPPEDWINDOW, FALSE);
		hWnd = CreateWindow(wc.lpszClassName, title, WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, adjustedRect.right - adjustedRect.left,
			adjustedRect.bottom - adjustedRect.top,
			NULL, NULL, GetModuleHandle(nullptr), 0);

		// Set Icon
		SendMessage(hWnd, WM_SETICON, ICON_SMALL,
			(LPARAM)LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_ICON1)));

	}
	else
	{
		// Create borderless window
		hWnd = CreateWindow(wc.lpszClassName, NULL, WS_POPUP,
			caveWindowClientRect.top, caveWindowClientRect.left, caveWindowClientRect.right - windowClientRect.left,
			caveWindowClientRect.bottom - caveWindowClientRect.top,
			NULL, NULL, GetModuleHandle(nullptr), 0);
		SetWindowLong(hWnd, GWL_STYLE, 0);
		SetCursor(NULL);
	}


#ifdef _DEBUG
	Console::RedirectIOToConsole();
#endif

	if (!caveMode)
	{
		// Initialize OVR before creating DX
		ovr_Initialize();
		hmd = ovrHmd_Create(0);
	}

	if (!hmd || MIRROR)
		ShowWindow(hWnd, SW_NORMAL);

	// Initialize audio
	OpenAL_Init();

	// Init avlib
	av_register_all();
	avcodec_register_all();
	av_log_set_level(AV_LOG_QUIET);

	// Load video
	int image_w, image_h, image_comp;
	stbi_uc* image_data = nullptr;
	if (argc > 1)
	{
		if (argc > 1)
		{
			printf("Opening %s\n", argv[1]);
			char* ext = PathFindExtensionA(argv[1]);
			if (strcmp(ext, ".png") == 0 || strcmp(ext, ".jpg") == 0)
			{
				media = MediaType::IMAGE;
			}
			if (strcmp(ext, ".mp4") == 0)
			{
				media = MediaType::VIDEO;
			}
		}

		// Load the texture
		if (media == MediaType::IMAGE)
		{
			image_data = stbi_load(argv[1], &image_w, &image_h, &image_comp, 4);
			assert(image_data != nullptr);
			texHeight = image_h;
			texWidth = image_w;
			bottomUp = image_h == image_w;
			frameTime = 1;

			static struct stat info;
			stat(argv[1], &info);
			pollThread = std::thread([&]{

				while (active)
				{
					struct stat tmp_info;
					stat(argv[1], &tmp_info);
					if (tmp_info.st_mtime > info.st_mtime)
					{
						image_data = stbi_load(argv[1], &image_w, &image_h, &image_comp, 4);
						if (image_data)
						{
							info = tmp_info;
							D3D11_MAPPED_SUBRESOURCE texmap;
							res = d3d_context->Map(d3d_texture[0], 0, D3D11_MAP_WRITE_DISCARD, 0, &texmap);
							memcpy(texmap.pData, image_data, image_w * image_h * image_comp);
							d3d_context->Unmap(d3d_texture[0], 0);
							printf("RELOAD\n");
						}
					}
					Sleep(1000);
				}
			});
		}
		if (media == MediaType::VIDEO)
		{
			player.load(argv[1]);
			texWidth = player.width;
			texHeight = player.height * 1.5f;
			bottomUp = player.width == player.height;
			frameTime = (double)player.video_stream->avg_frame_rate.den /
				(double)player.video_stream->avg_frame_rate.num;
		}
	}
	else
		exit(0);

	// Select the adapter with more memory
	IDXGIAdapter* selected_pAdapter = nullptr;
	{
		IDXGIFactory* pFactory;
		CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)(&pFactory));
		uint32 adapterIndex = 0;
		IDXGIAdapter* pAdapter;
		int maxMemory = 0;
		// Iterate through adapters.
		while (pFactory->EnumAdapters(adapterIndex, &pAdapter) != DXGI_ERROR_NOT_FOUND)
		{
			uint32 outputIndex = 0;
			IDXGIOutput* pOutput;
			
			DXGI_ADAPTER_DESC desc;
			res = pAdapter->GetDesc(&desc);
			assert(SUCCEEDED(res));
			int mem = desc.DedicatedVideoMemory / 1024u / 1024u;
			wprintf(L"Available Adapter: %s - %uMB\n", desc.Description, mem);

			if (mem > maxMemory)
			{
				selected_pAdapter = pAdapter;
				maxMemory = mem;
			}

			adapterIndex++;
		}
	}

	{
		DXGI_ADAPTER_DESC desc;
		res = selected_pAdapter->GetDesc(&desc);
		assert(SUCCEEDED(res));
		int mem = desc.DedicatedVideoMemory / 1024u / 1024u;
		wprintf(L"Selected Adapter: %s - %uMB\n", desc.Description, mem);
	}


	// Create swapchain
	{
		DXGI_SWAP_CHAIN_DESC desc{ 0 };
		desc.BufferCount = 2;
		if (!caveMode)
		{
			desc.BufferDesc.Width = windowClientRect.right;
			desc.BufferDesc.Height = windowClientRect.bottom;
		}
		else
		{
			desc.BufferDesc.Width = caveWindowClientRect.right;
			desc.BufferDesc.Height = caveWindowClientRect.bottom;
		}
		desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.BufferDesc.RefreshRate.Numerator = 0;
		desc.BufferDesc.RefreshRate.Denominator = 1;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.OutputWindow = hWnd;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		desc.Flags = 0;
		desc.Windowed = TRUE;
		res = D3D11CreateDeviceAndSwapChain(selected_pAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0, nullptr, 0,
			D3D11_SDK_VERSION, &desc, &dx_swapchain, &d3d_device, nullptr, &d3d_context);
		assert(SUCCEEDED(res));

		// If the swap chain already exists, resize it.
		//res = dx_swapchain->ResizeBuffers(
		//    2, // Double-buffered swap chain.
		//    static_cast<UINT>(windowClientRect.right),
		//    static_cast<UINT>(windowClientRect.bottom),
		//    DXGI_FORMAT_B8G8R8A8_UNORM,
		//    0 // Enable GetFrameLatencyWaitableObject().
		//    );
		//assert(SUCCEEDED(res));
		//dx_swapchain2 = reinterpret_cast<IDXGISwapChain2*>(dx_swapchain);
		//assert(dx_swapchain2 != nullptr);
		//hSwapchainWait = dx_swapchain2->GetFrameLatencyWaitableObject();
		//assert(hSwapchainWait != NULL);
	}

	// Print adapter description
	{
		IDXGIDevice* pDXGIDevice = nullptr;
		res = d3d_device->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice);
		assert(SUCCEEDED(res));

		IDXGIAdapter1 * pDXGIAdapter = nullptr;
		res = pDXGIDevice->GetParent(__uuidof(IDXGIAdapter1), (void **)&pDXGIAdapter);
		assert(SUCCEEDED(res));

		DXGI_ADAPTER_DESC desc;
		res = pDXGIAdapter->GetDesc(&desc);
		assert(SUCCEEDED(res));

		wprintf(L"Adapter: %s\n", desc.Description);
	}


	// Create color framebuffer
	{
		ID3D11Texture2D* colorTex{ nullptr };
		res = dx_swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&colorTex);
		assert(res == S_OK);
		res = d3d_device->CreateRenderTargetView(colorTex, nullptr, &d3d_backbuffer);
		assert(res == S_OK);
		colorTex->Release();
	}

	// Create depth
	{
		D3D11_TEXTURE2D_DESC depthDesc{ 0 };
		if (!caveMode)
		{
			depthDesc.Width = windowClientRect.right;
			depthDesc.Height = windowClientRect.bottom;
		}
		else
		{
			depthDesc.Width = caveWindowClientRect.right;
			depthDesc.Height = caveWindowClientRect.bottom;
		}
		depthDesc.ArraySize = 1;
		depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		depthDesc.CPUAccessFlags = 0;
		depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthDesc.MipLevels = 1;
		depthDesc.MiscFlags = 0;
		depthDesc.SampleDesc.Count = 1;
		depthDesc.SampleDesc.Quality = 0;
		depthDesc.Usage = D3D11_USAGE_DEFAULT;
		ID3D11Texture2D* depthTex{ nullptr };
		res = d3d_device->CreateTexture2D(&depthDesc, nullptr, &depthTex);
		assert(res == S_OK);
		res = d3d_device->CreateDepthStencilView(depthTex, nullptr, &d3d_depth);
		assert(res == S_OK);

		D3D11_DEPTH_STENCIL_DESC dsd{ 0 };
		dsd.DepthEnable = TRUE;
		dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		dsd.DepthFunc = D3D11_COMPARISON_LESS;
		dsd.StencilEnable = FALSE;
		res = d3d_device->CreateDepthStencilState(&dsd, &d3d_depthState);
		assert(res == S_OK);
	}

	D3D11_RASTERIZER_DESC rd;
	ZeroMemory(&rd, sizeof(rd));
	rd.AntialiasedLineEnable = FALSE;
	rd.CullMode = D3D11_CULL_NONE;
	rd.DepthBias = 0;
	rd.DepthBiasClamp = .0f;
	rd.DepthClipEnable = TRUE;
	rd.FillMode = D3D11_FILL_SOLID;
	rd.FrontCounterClockwise = FALSE;
	rd.MultisampleEnable = FALSE;
	rd.ScissorEnable = FALSE;
	rd.SlopeScaledDepthBias = .0f;
	res = d3d_device->CreateRasterizerState(&rd, &d3d_rasterizer);
	assert(res == S_OK);

	if (media == MediaType::VIDEO)
	{
		res = d3d_device->CreatePixelShader(YUV_PixelShader, sizeof(YUV_PixelShader), nullptr, &d3d_pixelShader);
		assert(res == S_OK);
		res = d3d_device->CreateVertexShader(YUV_VertexShader, sizeof(YUV_VertexShader), nullptr, &d3d_vertexShader);
		assert(res == S_OK);
	}
	else if (media == MediaType::IMAGE)
	{
		res = d3d_device->CreatePixelShader(RGB_PixelShader, sizeof(RGB_PixelShader), nullptr, &d3d_pixelShader);
		assert(res == S_OK);
		res = d3d_device->CreateVertexShader(RGB_VertexShader, sizeof(RGB_VertexShader), nullptr, &d3d_vertexShader);
		assert(res == S_OK);
	}

	D3D11_BUFFER_DESC matrix_desc{ 0 };
	matrix_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	matrix_desc.Usage = D3D11_USAGE_DEFAULT;
	matrix_desc.ByteWidth = sizeof(PerFrameBuffer);
	matrix_desc.CPUAccessFlags = 0;
	res = d3d_device->CreateBuffer(&matrix_desc, nullptr, &d3d_perFrameBuffer);
	assert(res == S_OK);

	{
		float aspect = windowClientRect.bottom / (float)windowClientRect.right;
		XMMATRIX proj = XMMatrixOrthographicOffCenterLH(-1, 1, -1, 1, 0, 10);
		PerAppBuffer perApp(proj);
		// size, usage, bind, cpu_access, misc, stride
		D3D11_BUFFER_DESC desc{ sizeof(PerAppBuffer), D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0 };
		D3D11_SUBRESOURCE_DATA data{ &perApp, 0, 0 };
		res = d3d_device->CreateBuffer(&desc, &data, &d3d_perAppBuffer);
		assert(res == S_OK);
	}

	SphereCreate(1, 100, 100);
	RectCreate();

	if (media == MediaType::VIDEO)
		res = d3d_device->CreateInputLayout(vertexLayoutDesc, _countof(vertexLayoutDesc), YUV_VertexShader, sizeof(YUV_VertexShader), &d3d_vertexLayout);
	if (media == MediaType::IMAGE)
		res = d3d_device->CreateInputLayout(vertexLayoutDesc, _countof(vertexLayoutDesc), RGB_VertexShader, sizeof(RGB_VertexShader), &d3d_vertexLayout);
	assert(res == S_OK);

	// from: https://msdn.microsoft.com/en-us/library/windows/desktop/ff476904(v=vs.85).aspx
	D3D11_TEXTURE2D_DESC tdesc{ 0 };
	tdesc.Width = texWidth;
	tdesc.Height = texHeight;
	tdesc.ArraySize = 1;
	tdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	tdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	tdesc.MipLevels = 1;
	tdesc.MiscFlags = 0;
	tdesc.SampleDesc.Count = 1;
	tdesc.SampleDesc.Quality = 0;
	tdesc.Usage = D3D11_USAGE_DYNAMIC;

	if (media == MediaType::VIDEO)
	{
		tdesc.Format = DXGI_FORMAT_R8_UNORM;
		for (int i = 0; i < _countof(d3d_texture); i++)
		{
			res = d3d_device->CreateTexture2D(&tdesc, nullptr, &d3d_texture[i]);
			assert(res == S_OK);
		}
	}
	else if (media == MediaType::IMAGE)
	{
		tdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		D3D11_SUBRESOURCE_DATA tsrd{ 0 };
		tsrd.pSysMem = image_data;
		tsrd.SysMemPitch = image_w * 4;
		res = d3d_device->CreateTexture2D(&tdesc, &tsrd, &d3d_texture[0]);
		assert(res == S_OK);
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvd;
	ZeroMemory(&srvd, sizeof(srvd));
	srvd.Format = tdesc.Format;
	srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvd.Texture2D.MipLevels = 1;

	if (media == MediaType::VIDEO)
	{
		for (int i = 0; i < _countof(d3d_textureView); i++)
		{
			res = d3d_device->CreateShaderResourceView(d3d_texture[i], &srvd, &d3d_textureView[i]);
			assert(res == S_OK);
		}
	}
	else if (media == MediaType::IMAGE)
	{
		res = d3d_device->CreateShaderResourceView(d3d_texture[0], &srvd, &d3d_textureView[0]);
		assert(res == S_OK);
	}


	if (media == MediaType::VIDEO)
	{
		// Map the frame to be filled by the decoder
		D3D11_MAPPED_SUBRESOURCE texmap;
		res = d3d_context->Map(d3d_texture[1 - frameIndex], 0, D3D11_MAP_WRITE_DISCARD, 0, &texmap);
		assert(res == S_OK);
		frameMappedData = (uint8_t*)texmap.pData;
	}

	D3D11_SAMPLER_DESC sdesc;
	ZeroMemory(&sdesc, sizeof(sdesc));
	sdesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sdesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sdesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sdesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sdesc.MipLODBias = 0.0f;
	sdesc.MaxAnisotropy = 1;
	sdesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	sdesc.BorderColor[0] = 0.0f;
	sdesc.BorderColor[1] = 0.0f;
	sdesc.BorderColor[2] = 0.0f;
	sdesc.BorderColor[3] = 0.0f;
	sdesc.MinLOD = -FLT_MAX;
	sdesc.MaxLOD = FLT_MAX;
	res = d3d_device->CreateSamplerState(&sdesc, &d3d_sampler);
	assert(res == S_OK);

	// Deferred context
	D3D11_FEATURE_DATA_THREADING threadingSupport;
	d3d_device->CheckFeatureSupport(D3D11_FEATURE_THREADING, &threadingSupport, sizeof(threadingSupport));
	if (threadingSupport.DriverConcurrentCreates && threadingSupport.DriverCommandLists)
		printf("DX11 Threading supported\n");
	else
		printf("DX11 Threading NOT supported\n");

	ID3D11DeviceContext* d3d_deferred{ nullptr };
	d3d_device->CreateDeferredContext(0, &d3d_deferred);

	// Init OVR
	if (hmd)
	{
		ovrHmd_AttachToWindow(hmd, hWnd, nullptr, nullptr);
		ovrHmd_SetEnabledCaps(hmd, 
			ovrHmdCap_LowPersistence
			| ovrHmdCap_DynamicPrediction
			| (ovrHmdCap_NoMirrorToWindow & !MIRROR)
			//| ovrHmdCap_NoVSync
			);
		ovrHmd_ConfigureTracking(hmd, ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection |
			ovrTrackingCap_Position, 0);

		// Make the eye render buffers (caution if actual size < requested due to HW limits). 
		for (int eye = 0; eye < 2; eye++)
		{
			idealSize = ovrHmd_GetFovTextureSize(hmd, (ovrEyeType)eye, hmd->DefaultEyeFov[eye], 1.0f);
			
			CreateRTT(eye, idealSize.w, idealSize.h);

			EyeRenderViewport[eye].Pos = OVR::Vector2i(0, 0);
			EyeRenderViewport[eye].Size = idealSize;
		}

		ovrD3D11Config d3d11cfg;
		d3d11cfg.D3D11.Header.API = ovrRenderAPI_D3D11;
		d3d11cfg.D3D11.Header.BackBufferSize = OVR::Sizei(hmd->Resolution.w, hmd->Resolution.h);
		d3d11cfg.D3D11.Header.Multisample = 1;
		d3d11cfg.D3D11.pDevice = d3d_device;
		d3d11cfg.D3D11.pDeviceContext = d3d_context;
		d3d11cfg.D3D11.pBackBufferRT = d3d_backbuffer;
		d3d11cfg.D3D11.pSwapChain = dx_swapchain;

		ovrHmd_ConfigureRendering(hmd, &d3d11cfg.Config, ovrDistortionCap_Chromatic | ovrDistortionCap_Vignette |
			ovrDistortionCap_TimeWarp | ovrDistortionCap_Overdrive, hmd->DefaultEyeFov, EyeRenderDesc);

		ovrHmd_DismissHSWDisplay(hmd);
	}
	else
	{
		if (!caveMode)
		{
			idealSize.w = windowClientRect.right / 2;
			idealSize.h = windowClientRect.bottom / 2;
		}
		else
		{
			idealSize.w = caveWindowClientRect.right / 2;
			idealSize.h = caveWindowClientRect.bottom / 2;
		}
		CreateRTT(0, idealSize.w, idealSize.h);
	}

	InitCAVE();
	InitD3D();

	std::thread decoder;
	if (media == MediaType::VIDEO)
		decoder = std::thread(decodeThread);

	MSG msg{ 0 };
	auto start = std::chrono::steady_clock::now();
	double elapsed{ 0 };
	double elapsed_decode{ 0 };
	int frames{ 0 };

	//std::thread tester([&]{
	//	while (active)
	//	{
	//		if (playing)
	//		{
	//			ALint val;
	//			
	//			alGetSourcei(player.audio_source, AL_BUFFERS_PROCESSED, &val);
	//			while (val-- > 1)
	//			{
	//				alSourceUnqueueBuffers(player.audio_source, 1, &player.audio_buffer_queue.back());
	//				alDeleteBuffers(1, &player.audio_buffer_queue.back());
	//				player.audio_buffer_queue.pop_back();
	//			}
	//			
	//			alGetSourcei(player.audio_source, AL_BUFFERS_QUEUED, &val);
	//			if (val == 0)
	//			{
	//				printf("AUDIO EMPTY");
	//			}
	//		}
	//		else
	//		{
	//			std::this_thread::sleep_for(std::chrono::milliseconds(100));
	//		}
	//	}
	//});

	std::ifstream file(cameraTrackCSV);

	while (true)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				break;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			auto stop = std::chrono::steady_clock::now();
			auto dt = std::chrono::duration<float>(stop - start).count();
			start = stop;
			elapsed += dt;
			elapsed_decode += dt;

			if (playing && elapsed > 1.)
			{
				sprintf_s(title, "%s - Playing at %d fps", windowTitle, frames);
				printf("%s\n", title);
				SetWindowText(hWnd, title);
				elapsed -= 1.;
				frames = 0;
			}

			Render(dt);
			frames++;

			if (elapsed_decode > frameTime)
			{
				// frameTime sets set to the correct time for this video => This if will be called once per VIDEO frame
				if (playing) {
					TrackCamera row;
					if (file >> row) {
						/*
						cameraPitch = atof(row[2].c_str());
						cameraYaw = atof(row[3].c_str());*/

						// not sure why I need to invert these?
						cameraX = atof(row[0].c_str()) * -1.0f;
						cameraY = atof(row[1].c_str()) * -1.0f;
						cameraZ = atof(row[2].c_str()) * -1.0f;
						cameraW = atof(row[3].c_str()) * -1.0f;

						double roll = 0.0;
					}
					else {
						// EOF reached
						cameraPitch = 0.0f;
						cameraYaw = 0.0f;

						cameraX = 0.0f;
						cameraY = 0.0f;
						cameraZ = 0.0f;
						cameraW = 0.0f;

						file.clear();
						file.seekg(0, ios::beg);
					}
				}

				//if (playing)
				//{
				//    D3D11_MAPPED_SUBRESOURCE map;
				//    res = d3d_context->Map(d3d_texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
				//    assert(SUCCEEDED(res));
				//    auto frame = player.decodeFrame(nullptr, (uint8_t*)map.pData);
				//    d3d_context->Unmap(d3d_texture, 0);
				//    //auto oldTex = d3d_texture;
				//    //auto oldView = d3d_textureView;
				//    //d3d_texture = frame.tex;
				//    //d3d_textureView = frame.view;
				//    //oldView->Release();
				//    //oldTex->Release();
				//}
				elapsed_decode -= frameTime;
			}
		}
	}

	active = false;
	if (media == MediaType::VIDEO)
		decoder.join();
	if (media == MediaType::IMAGE)
		pollThread.join();

	return 0;
}

