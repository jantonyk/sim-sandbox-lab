// Gallery template, based on the Microsoft DX11 tutorial 04

#include <windows.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>
#include <atlbase.h>
#include <fstream>
#include <vector>
#include "Resource.h"

using namespace DirectX;

#define COMPILE_CSO

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct SimpleVertex
{
	XMFLOAT3 Pos;
	XMFLOAT4 Color;
};


struct ConstantBuffer
{
	XMMATRIX mWorld;
	XMMATRIX mView;
	XMMATRIX mProjection;
};


class D3DFramework final {

	D3D_DRIVER_TYPE _driverType = D3D_DRIVER_TYPE_NULL;
	D3D_FEATURE_LEVEL _featureLevel = D3D_FEATURE_LEVEL_11_1;
	HINSTANCE _hInst = nullptr;
	HWND _hWnd = nullptr;
	CComPtr <ID3D11Device> _pd3dDevice;
	CComPtr <ID3D11Device1> _pd3dDevice1;
	CComPtr <ID3D11DeviceContext> _pImmediateContext;
	CComPtr <ID3D11DeviceContext1> _pImmediateContext1;
	CComPtr <IDXGISwapChain1> _swapChain;
	CComPtr <IDXGISwapChain1> _swapChain1;
	CComPtr <ID3D11RenderTargetView> _pRenderTargetView;
	CComPtr <ID3D11VertexShader> _pVertexShader;
	CComPtr <ID3D11PixelShader> _pPixelShader;
	CComPtr <ID3D11InputLayout> _pVertexLayout;
	CComPtr <ID3D11Buffer> _pVertexBuffer;
	CComPtr <ID3D11Buffer> _pIndexBuffer;
	CComPtr <ID3D11Buffer> _pConstantBuffer;
	XMMATRIX _World = {};
	XMMATRIX _View = {};
	XMMATRIX _Projection = {};

public:

	D3DFramework() = default;
	D3DFramework(D3DFramework&) = delete;
	D3DFramework(D3DFramework&&) = delete;
	D3DFramework& operator=(const D3DFramework&) = delete;
	D3DFramework& operator=(const D3DFramework&&) = delete;
	
	//--------------------------------------------------------------------------------------
	// Called every time the application receives a message
	//--------------------------------------------------------------------------------------
	static LRESULT CALLBACK wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
		std::string msg;
		
		switch (message) {
		case WM_PAINT:
			PAINTSTRUCT ps;
			BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		case WM_KEYDOWN:
			switch (wParam) {
			case 'A':
				msg = "A pressed";
				break;
			case 'B':
				if (GetKeyState(VK_CONTROL) < 0) {
					msg = "CTRL B pressed";
				}
				else {
					msg = "B pressed";
				}
				break;
			case VK_LEFT:
				msg = "Left cursor pressed";
				break;
			case VK_F1:
				msg = "F1 pressed";
				break;
			default:
				break;
			}
			break;

		case WM_KEYUP: 
			switch (wParam) {
			case 'A':
				msg = "A released";
				break;
			case '1':
				msg = "1 released";
				break;
			default:
				break;
			}
				
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}

		return 0;
	}


	//--------------------------------------------------------------------------------------
	// Register class and create window
	//--------------------------------------------------------------------------------------
	HRESULT initWindow(HINSTANCE hInstance, int nCmdShow) {
		// Register class
		WNDCLASSEX wcex;
		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = wndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hInstance;
		wcex.hIcon = LoadIcon(hInstance, reinterpret_cast<LPCTSTR>(IDI_GALLERY));
		wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = L"Starter Template";
		wcex.hIconSm = LoadIcon(wcex.hInstance, reinterpret_cast<LPCTSTR>(IDI_GALLERY));
		if (!RegisterClassEx(&wcex))
			return E_FAIL;

		// Create window
		_hInst = hInstance;
		RECT rc = { 0, 0, 800, 600 };
		AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
		_hWnd = CreateWindow(L"Starter Template", L"Direct3D 11 Simulation",
			WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
			CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
			nullptr);
		if (!_hWnd)
			return E_FAIL;

		ShowWindow(_hWnd, nCmdShow);

		return S_OK;
	}


	//--------------------------------------------------------------------------------------
	// Helper for compiling shaders with D3DCompile
	//
	// With VS 11, we could load up prebuilt .cso files instead...
	//--------------------------------------------------------------------------------------
	static HRESULT compileShaderFromFile(const WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
	{
		auto dwShaderFlags = static_cast<DWORD>(D3DCOMPILE_ENABLE_STRICTNESS);
#ifdef _DEBUG
		// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
		// Setting this flag improves the shader debugging experience, but still allows 
		// the shaders to be optimized and to run exactly the way they will run in 
		// the release configuration of this program.
		dwShaderFlags |= D3DCOMPILE_DEBUG;

		// Disable optimizations to further improve shader debugging
		dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

		CComPtr <ID3DBlob> pErrorBlob;
		const auto hr = D3DCompileFromFile(szFileName, nullptr, nullptr, szEntryPoint, szShaderModel, dwShaderFlags, 0, ppBlobOut, &pErrorBlob);
		if (FAILED(hr)) {
			if (pErrorBlob)
				OutputDebugStringA(static_cast<const char*>(pErrorBlob->GetBufferPointer()));
			return hr;
		}

		return S_OK;
	}


	//--------------------------------------------------------------------------------------
	// Create Direct3D device and swap chain
	//--------------------------------------------------------------------------------------
	HRESULT initDevice()
	{
		auto hr = S_OK;

		RECT rc;
		GetClientRect(_hWnd, &rc);
		UINT width = rc.right - rc.left;
		UINT height = rc.bottom - rc.top;

		UINT createDeviceFlags = 0;
#ifdef _DEBUG
		createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

		D3D_DRIVER_TYPE driverTypes[] = {
			D3D_DRIVER_TYPE_HARDWARE,
			D3D_DRIVER_TYPE_WARP,
			D3D_DRIVER_TYPE_REFERENCE,
		};
		auto numDriverTypes = ARRAYSIZE(driverTypes);

		D3D_FEATURE_LEVEL featureLevels[] = {
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0,
		};
		auto numFeatureLevels = static_cast<UINT>(ARRAYSIZE(featureLevels));

		for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++) {
			_driverType = driverTypes[driverTypeIndex];
			hr = D3D11CreateDevice(nullptr, _driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels, D3D11_SDK_VERSION, &_pd3dDevice, &_featureLevel, &_pImmediateContext);

			if (hr == E_INVALIDARG)
				// DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
				hr = D3D11CreateDevice(nullptr, _driverType, nullptr, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1, D3D11_SDK_VERSION, &_pd3dDevice, &_featureLevel, &_pImmediateContext);

			if (SUCCEEDED(hr))
				break;
		}
		if (FAILED(hr))
			return hr;

		// Obtain DXGI factory from device (since we used nullptr for pAdapter above)
		CComPtr <IDXGIFactory1> dxgiFactory;
		{
			CComPtr <IDXGIDevice> dxgiDevice;
			hr = _pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
			if (SUCCEEDED(hr)) {
				CComPtr <IDXGIAdapter> adapter;
				hr = dxgiDevice->GetAdapter(&adapter);
				if (SUCCEEDED(hr)) {
					hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory));
				}
			}
		}
		if (FAILED(hr))
			return hr;

		// Create swap chain
		CComPtr <IDXGIFactory2> dxgiFactory2;
		hr = dxgiFactory->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory2));

		// DirectX 11.1 or later
		hr = _pd3dDevice->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&_pd3dDevice1));
		if (SUCCEEDED(hr)) {
			_pImmediateContext->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&_pImmediateContext1));
		}

		DXGI_SWAP_CHAIN_DESC1 sd{};
		sd.Width = width;
		sd.Height = height;
		sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount = 1;

		hr = dxgiFactory2->CreateSwapChainForHwnd(_pd3dDevice, _hWnd, &sd, nullptr, nullptr, &_swapChain1);
		if (SUCCEEDED(hr)) {
			hr = _swapChain1->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(&_swapChain));
		}

		// Note this tutorial doesn't handle full-screen swap chains so we block the ALT+ENTER shortcut
		dxgiFactory->MakeWindowAssociation(_hWnd, DXGI_MWA_NO_ALT_ENTER);

		if (FAILED(hr))
			return hr;

		// Create a render target view
		CComPtr <ID3D11Texture2D> pBackBuffer;
		hr = _swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
		if (FAILED(hr))
			return hr;

		hr = _pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &_pRenderTargetView);
		if (FAILED(hr))
			return hr;

		_pImmediateContext->OMSetRenderTargets(1, &_pRenderTargetView.p, nullptr);

		// Setup the viewport
		D3D11_VIEWPORT vp;
		vp.Width = static_cast<FLOAT>(width);
		vp.Height = static_cast<FLOAT>(height);
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		_pImmediateContext->RSSetViewports(1, &vp);

		// Define the input layout
		D3D11_INPUT_ELEMENT_DESC layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		auto numElements = static_cast<UINT>(ARRAYSIZE(layout));

#ifdef COMPILE_CSO
		// Compile the vertex shader
		CComPtr <ID3DBlob> pVSBlob;
		hr = compileShaderFromFile(L"Simulation.fx", "VS", "vs_5_0", &pVSBlob);
		if (FAILED(hr)) {
			MessageBox(nullptr,	L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
			return hr;
		}

		// Create the vertex shader
		hr = _pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &_pVertexShader);
		if (FAILED(hr)) {
			return hr;
		}

		// Create the input layout
		hr = _pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &_pVertexLayout);
		if (FAILED(hr))
			return hr;
#else
		{
			const std::string fileName{ "..\\Debug\\Simulation_VS.cso" };
			std::ifstream fin(fileName, std::ios::binary);
			if (!fin) {
				MessageBox(nullptr, L"The CSO file cannot be found.", L"Error", MB_OK);
				return E_FAIL;
			}
			std::vector<unsigned char> byteCode(std::istreambuf_iterator<char>(fin), {});

			hr = _pd3dDevice->CreateVertexShader(&byteCode[0], byteCode.size(), nullptr, &_pVertexShader);
			if (FAILED(hr)) {
				return hr;
			}

			// Create the input layout
			hr = _pd3dDevice->CreateInputLayout(layout, numElements, &byteCode[0], byteCode.size(), &_pVertexLayout);
			if (FAILED(hr))
				return hr;
		}
#endif

		// Set the input layout
		_pImmediateContext->IASetInputLayout(_pVertexLayout);

#ifdef COMPILE_CSO
		// Compile the pixel shader
		CComPtr <ID3DBlob> pPSBlob;
		hr = compileShaderFromFile(L"Simulation.fx", "PS", "ps_5_0", &pPSBlob);
		if (FAILED(hr)) {
			MessageBox(nullptr,	L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
			return hr;
		}

		// Create the pixel shader
		hr = _pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &_pPixelShader);
		if (FAILED(hr))
			return hr;

#else
		{
			const std::string fileName { "..\\Debug\\Simulation_PS.cso" };
			std::ifstream fin(fileName, std::ios::binary);
			if (!fin) {
				MessageBox(nullptr, L"The CSO file cannot be found.", L"Error", MB_OK);
				return E_FAIL;
			}
			std::vector<unsigned char> byteCode(std::istreambuf_iterator<char>(fin), {});

			hr = _pd3dDevice->CreatePixelShader(&byteCode[0], byteCode.size(), nullptr, &_pPixelShader);
			if (FAILED(hr)) {
				return hr;
			}
		}
#endif		

		// Create vertex buffer
		SimpleVertex vertices[] = {
			{ XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
			{ XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
			{ XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) },
			{ XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
			{ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) },
			{ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
			{ XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) },
			{ XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f) },
		};
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(SimpleVertex) * 8;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;

		D3D11_SUBRESOURCE_DATA InitData = {};
		InitData.pSysMem = vertices;
		hr = _pd3dDevice->CreateBuffer(&bd, &InitData, &_pVertexBuffer);
		if (FAILED(hr))
			return hr;

		// Set vertex buffer
		const auto stride = static_cast<UINT>(sizeof(SimpleVertex));
		const auto offset = static_cast<UINT>(0);
		_pImmediateContext->IASetVertexBuffers(0, 1, &_pVertexBuffer.p, &stride, &offset);

		// Create index buffer
		WORD indices[] = {
			3,1,0,
			2,1,3,

			0,5,4,
			1,5,0,

			3,4,7,
			0,4,3,

			1,6,5,
			2,6,1,

			2,7,6,
			3,7,2,

			6,4,5,
			7,4,6,
		};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(WORD) * 36;        // 36 vertices needed for 12 triangles in a triangle list
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.CPUAccessFlags = 0;
		InitData.pSysMem = indices;
		hr = _pd3dDevice->CreateBuffer(&bd, &InitData, &_pIndexBuffer);
		if (FAILED(hr))
			return hr;

		// Set index buffer
		_pImmediateContext->IASetIndexBuffer(_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

		// Set primitive topology
		_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Create the constant buffer
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(ConstantBuffer);
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = 0;
		hr = _pd3dDevice->CreateBuffer(&bd, nullptr, &_pConstantBuffer);
		if (FAILED(hr))
			return hr;

		// Initialize the world matrix
		_World = XMMatrixIdentity();

		// Initialize the view matrix
		const XMVECTOR Eye = XMVectorSet(0.0f, 1.0f, -5.0f, 0.0f);
		const XMVECTOR At = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		const XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		_View = XMMatrixLookAtLH(Eye, At, Up);

		// Initialize the projection matrix
		_Projection = XMMatrixPerspectiveFovLH(XM_PIDIV2, width / static_cast<FLOAT>(height), 0.01f, 100.0f);

		return S_OK;
	}


	//--------------------------------------------------------------------------------------
	// Clean up the objects we've created
	//--------------------------------------------------------------------------------------
	~D3DFramework() {
		try {
			if (_pImmediateContext)
				_pImmediateContext->ClearState();
		}
		catch (...) {

		}
	}


	//--------------------------------------------------------------------------------------
	// Render a frame
	//--------------------------------------------------------------------------------------
	void render() {
		// Update our time
		static float t = 0.0f;
		if (_driverType == D3D_DRIVER_TYPE_REFERENCE) {
			t += static_cast<float>(XM_PI) * 0.0125f;
		}
		else {
			static ULONGLONG timeStart = 0;
			const ULONGLONG timeCur = GetTickCount64();
			if (timeStart == 0)
				timeStart = timeCur;
			t = static_cast<float>(timeCur - timeStart) / 1000.0f;
		}

		//
		// Animate the cube
		//
		_World = XMMatrixRotationY(t);

		//
		// Clear the back buffer
		//
		_pImmediateContext->ClearRenderTargetView(_pRenderTargetView, Colors::MidnightBlue);

		//
		// Update variables
		//
		const ConstantBuffer cb{ XMMatrixTranspose(_World), XMMatrixTranspose(_View), XMMatrixTranspose(_Projection) };
		_pImmediateContext->UpdateSubresource(_pConstantBuffer, 0, nullptr, &cb, 0, 0);

		//
		// Renders a triangle
		//
		_pImmediateContext->VSSetShader(_pVertexShader, nullptr, 0);
		_pImmediateContext->VSSetConstantBuffers(0, 1, &_pConstantBuffer.p);
		_pImmediateContext->PSSetShader(_pPixelShader, nullptr, 0);
		_pImmediateContext->DrawIndexed(36, 0, 0);        // 36 vertices needed for 12 triangles in a triangle list

		//
		// Present our back buffer to our front buffer
		//
		_swapChain->Present(0, 0);
	}

};

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int nShowCmd) {

	D3DFramework app;

	if (FAILED(app.initWindow(hInstance, nShowCmd)))
		return 0;

	if (FAILED(app.initDevice())) 
		return 0;

	// Main message loop
	MSG msg;
	msg.message = 0;
	while (WM_QUIT != msg.message) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			app.render();
		}
	}

	return static_cast<int>(msg.wParam);
}