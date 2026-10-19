#include "../src/pch.h"
#include "../src/ztl/ztl.h"
#include "../src/mountinfo_render.h"
#include <filesystem>
#include <cstdio>
static ZAllocEx<ZAllocAnonSelector> testAllocator;
ZALLOCEX(ZAllocAnonSelector, &testAllocator)
extern "C" void* __stdcall ZtlTaskMemAllocImp(SIZE_T size) { return CoTaskMemAlloc(size); }
extern "C" void __stdcall ZtlTaskMemFreeImp(void* p) { CoTaskMemFree(p); }
#pragma comment(linker, "/EXPORT:ZtlTaskMemAllocImp=_ZtlTaskMemAllocImp@4")
#pragma comment(linker, "/EXPORT:ZtlTaskMemFreeImp=_ZtlTaskMemFreeImp@4")


int wmain(int argc, wchar_t** argv) {
    if (argc != 2 && argc != 3) return 2;
    SetDllDirectoryW(argv[1]);
    HMODULE pcom = LoadLibraryW((std::filesystem::path(argv[1]) / L"PCOM.dll").c_str());
    if (!pcom) return 3;
    auto init = reinterpret_cast<HRESULT(__cdecl*)()>(GetProcAddress(pcom, "PcInitModule"));
    if (!init || FAILED(init())) return 4;
    FARPROC api[5] = {GetProcAddress(pcom, "PcCreateObject"), nullptr, nullptr, nullptr, GetProcAddress(pcom, "PcRootNameSpace")};
    _g_ppfnPCOMAPIs = api;
    try {
        IWzNameSpacePtr root; IWzResManPtr rm; IWzFileSystemPtr fs;
        PcCreateObject<IWzNameSpacePtr>(L"NameSpace", root, nullptr);
        PcCreateObject<IWzResManPtr>(L"ResMan", rm, nullptr);
        PcCreateObject<IWzFileSystemPtr>(L"NameSpace#FileSystem", fs, nullptr);
        PcSetRootNameSpace(root); rm->rootNameSpace = root;
        rm->SetResManParam(static_cast<RESMAN_PARAM>(RC_AUTO_REPARSE | RC_AUTO_SERIALIZE), -1, -1);
        fs->Init((std::filesystem::path(argv[1]) / L"Data").c_str()); root->Mount(L"/", fs, 0);
        IWzCanvasPtr bg = rm->GetObjectA(L"UI/UIWindow.img/UserInfo/backgrnd5").GetUnknown();
        std::vector<std::vector<MountPreviewPart>> animation;
        MountBounds bounds{0,0,0,0}; bool first = true;
        for (int i=0; i<12; ++i) {
            std::vector<MountPreviewPart> parts;
            for (int j=0; j<3; ++j) {
                wchar_t path[160]; swprintf_s(path,L"Character/TamingMob/01902002.img/stand1/%d/%d",i,j);
                IWzCanvasPtr c=rm->GetObjectA(path).GetUnknown(); if (!c) return 5;
                MountPreviewPart p{c,int(c->width),int(c->height),int(c->cx),int(c->cy),j==0?2:j==1?1:0};
                AlignMountPart(p);
                if(i==0 && j==0 && (p.cx!=72 || p.cy!=47)) return 12;
                if(i==0 && j==1 && (p.cx!=49 || p.cy!=13)) return 13;
                MountBounds b{double(-p.cx),double(-p.cy),double(p.w-p.cx),double(p.h-p.cy)};
                if(first){bounds=b;first=false;}else{
                    bounds.left=(std::min)(bounds.left,b.left);bounds.top=(std::min)(bounds.top,b.top);
                    bounds.right=(std::max)(bounds.right,b.right);bounds.bottom=(std::max)(bounds.bottom,b.bottom);
                }
                parts.push_back(p);
            }
            std::stable_sort(parts.begin(),parts.end(),[](const auto&a,const auto&b){return a.order<b.order;});
            animation.push_back(std::move(parts));
        }
        auto layout=FitMountPreview(bounds); if(layout.scale>=1||layout.scale<=0) return 6;
        IWzCanvasPtr reused; PcCreateObject<IWzCanvasPtr>(L"Canvas",reused,nullptr);
        reused->Create(87,77,0,CANVAS_PIXFORMAT::CP_A8R8G8B8);
        unsigned differingFrames=0; unsigned previous=0;
        int frameIndex=0;
        for(const auto&parts:animation){
            auto frame=ComposeMountPreview(bg,parts,layout);
            if(frame->width!=87||frame->height!=77) return 7;
            reused->Copy(0,0,frame);
            if(argc==3 && frameIndex==0){
                BITMAPFILEHEADER file{}; BITMAPINFOHEADER info{};
                file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+87*77*4;
                info.biSize=sizeof(info);info.biWidth=87;info.biHeight=77;info.biPlanes=1;info.biBitCount=32;
                FILE*out=nullptr;if(!_wfopen_s(&out,argv[2],L"wb")&&out){
                    fwrite(&file,sizeof(file),1,out);fwrite(&info,sizeof(info),1,out);
                    for(int y=76;y>=0;--y)for(int x=0;x<87;++x){unsigned pixel=frame->Getpixel(x,y);fwrite(&pixel,4,1,out);}fclose(out);
                }
            }
            ++frameIndex;
            unsigned checksum=0,changed=0;
            for(int y=0;y<77;++y) for(int x=0;x<87;++x){
                unsigned pixel=frame->Getpixel(x,y);
                if(reused->Getpixel(x,y)!=pixel) return 8;
                changed+=pixel!=bg->Getpixel(x+9,y+182);checksum=checksum*33+pixel;
            }
            if(changed<50) return 9;
            differingFrames+=checksum!=previous;previous=checksum;
        }
        if(differingFrames<2)return 10;
        std::puts("PASS: 12 full-body frames / 36 parts; complete frame replacement leaves no prior-frame pixels");
        std::fflush(stdout);std::_Exit(0);
    }catch(const _com_error&e){std::printf("COM %08X\n",e.Error());return 11;}
}
