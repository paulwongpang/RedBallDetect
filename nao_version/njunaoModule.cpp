//
//  njunaoModule.cpp
//  RedBallDetect
//
//  Created by Fanming Luo on 2017/11/19.
//  Copyright © 2017年 Fanming Luo. All rights reserved.
//
#include "main.hpp"
#include <vector>
#include <qi/log.hpp>
#include <alvision/alimage.h>
#include <alproxies/almemoryproxy.h>
#include <alproxies/altexttospeechproxy.h>

using namespace AL;
njunaoModule::njunaoModule(boost::shared_ptr<ALBroker> broker,const std::string& name):
  ALModule(broker, name),
    fMemProxy(getParentBroker()),
  fRegisteredToVideoDevice(false),
  fIplImageHeader(cv::Mat()),
    Detecting(false),
    StartDetect(false),
    RefeshingFlag(false),
    PoleDetecting(false),
    expect_camera(0),
tts(getParentBroker())
{
    setModuleDescription( "A module write by NJUer for contest"
                         "This is an example of a generic video module. "
                         "It can be used to save images returned by the camera. "
                         "It internally uses OpenCv's cvSaveImage function. "
                         "Refer to the Example codes section below for an example of use.");
    
    // Define the module example.
    addModuleExample( "Python",
                     "  # Create a proxy to the module \n"
                     "  sampleProxy = ALProxy('njunaoModule', '127.0.0.1', 9559)\n\n"
                     "  # Register our module to the Video Input Module. \n"
                     "  sampleProxy.registerToVideoDevice(1, 13)\n\n"
                     "  # Save image in remote mode. \n"
                     "  sampleProxy.saveImageLocal('/home/nao/img.jpg') \n\n"
                     "  # Unregister.\n"
                     "  sampleProxy.unRegisterFromVideoDevice()\n"
                     );
    
    // Define bound methods with their description.
    functionName( "registerToVideoDevice", getName(), "Register to the V.I.M." );
    BIND_METHOD( njunaoModule::registerToVideoDevice );
    
    functionName( "unRegisterFromVideoDevice", getName(), "Unregister from the V.I.M." );
    BIND_METHOD( njunaoModule::unRegisterFromVideoDevice );
    
    functionName( "saveImageLocal", getName(), "Save an image received from the camera." );
    BIND_METHOD( njunaoModule::saveImageLocal );
    
    functionName( "RedBallFind", getName(), "Find the red ball on the green ground." );
    BIND_METHOD( njunaoModule::RedBallFind );
    
    functionName( "ContinuousFindBall", getName(), "Continuous Find RedBall" );
    BIND_METHOD( njunaoModule::ContinuousFindBall );
    
    functionName( "PoleFind", getName(), "Find the yellow Pole on the green ground." );
    BIND_METHOD( njunaoModule::PoleFind );
    
    functionName( "ContinuousFindPole", getName(), "Continuous Find Pole" );
    BIND_METHOD( njunaoModule::ContinuousFindPole );
    
    functionName( "sayVersion", getName(), "tell what version is it" );
    BIND_METHOD( njunaoModule::sayVersion );
    
    functionName( "ContinuousRefreshCam", getName(), "Refresh Camera" );
    BIND_METHOD( njunaoModule::ContinuousRefreshCam );
    
    functionName( "ChangeCamera", getName(), "Change Camera index" );
    BIND_METHOD( njunaoModule::ChangeCamera );
}

njunaoModule::~njunaoModule()
{
    try
    {
        if(fCamProxy)
            fCamProxy->unsubscribe(fVideoClientName);
        if (StartDetect)StartDetect=false;
        fCamProxy.reset();
    }
    catch(const AL::ALError& e)
    {
        qiLogError("vision.njunaoModule") <<  e.toString() << std::endl;
    }
}

void njunaoModule::registerToVideoDevice(const int &pResolution, const int &pColorSpace)
{
    if (fRegisteredToVideoDevice) {
        return;
    }
    kOriginalName = "njuRedBallmodule";
    Reso=pResolution;
    pcs=pColorSpace;
    const int kImgDepth = 8;
    const int kFps = 30;
    int type;
    int imgNbLayers = 0;
    if (!fIplImageHeader.empty()) fIplImageHeader.release();
    setSizeFromResolution(pResolution, imgWidth, imgHeight);
    imgNbLayers = getNumLayersInColorSpace(pColorSpace);
    type = (imgNbLayers) == 3 ? CV_8UC3 : CV_8UC1;
    fIplImageHeader = cv::Mat(cv::Size(imgWidth, imgHeight), type);
    if(fCamProxy)
        fVideoClientName = fCamProxy->subscribe(kOriginalName, pResolution, pColorSpace, kFps );
    qiLogInfo("vision.genericvideomodule") << "Module registered as " << fVideoClientName << std::endl;
    fRegisteredToVideoDevice = true;
    StartDetect=true;
    fMemProxy.insertData("njunaoBallPositionStopFlag",0);
    fMemProxy.insertData("njunaoPolePositionStopFlag",0);
    Detecting=false;
}

void njunaoModule::unRegisterFromVideoDevice()
{
    StartDetect=false;
    if (!fIplImageHeader.empty())
        fIplImageHeader.release();
    qiLogInfo("vision.njunaoModule") << "try to unregister " << fVideoClientName << " module." << std::endl;
    if (!fRegisteredToVideoDevice) {return;}
    if(fCamProxy)
        fCamProxy->unsubscribe(fVideoClientName);
    fRegisteredToVideoDevice = false;
    RefeshingFlag=false;
}

void njunaoModule::saveImageLocal(const std::string& pName)
{
    if (!fRegisteredToVideoDevice) {
        throw ALError(getName(), "saveImageLocal()",  "No video module is currently "
                      "registered! Call registerToVideoDevice() first.");
    }
   // RefeshMat();
    cv::imwrite(pName,fIplImageHeader);
}

void njunaoModule::RefeshMat()
{
    ALImage* imageIn = NULL;
    imageIn = (ALImage*)fCamProxy->getImageLocal(fVideoClientName);
    if (!imageIn) {
        throw ALError(getName(), "saveImageLocal", "Invalid image returned.");
    }
    // You can get some image information that you may find useful.
    //const int width = imageIn->fWidth;
    //const int height = imageIn->fHeight;
    //const int nbLayers = imageIn->fNbLayers;
    //const int colorSpace = imageIn->fColorSpace;
    //const long long timeStamp = imageIn->getTimeStamp();
    //const int seconds = (int)(timeStamp/1000000LL);
    fIplImageHeader.data = imageIn->getData();
    fCamProxy->releaseImage(fVideoClientName);
}

std::vector<float> njunaoModule::RedBallFind()
{

    std::vector<float> PosTrans;
    PosTrans.push_back(0);
    PosTrans.push_back(0);
    if ((!Detecting)&&RefeshingFlag){
        if (StartDetect)
        {
            Detecting=true;
    //       RefeshMat();
            std::vector<float> RedBallPosition=DetectRedBall(fIplImageHeader);
   

            if (!(RedBallPosition[0]==0&&RedBallPosition[1]==0&&RedBallPosition[2]==0))
            {
                if (!(imgWidth==0||imgHeight==0))
                {
                    PosTrans[0]=RedBallPosition[0]/(1.0f*imgWidth);
                    PosTrans[1]=RedBallPosition[1]/(1.0f*imgHeight);
                }
            }
            fMemProxy.insertData("njunaoBallPosition",PosTrans);
            fMemProxy.raiseEvent("njuFindBall",PosTrans);
        }
        Detecting=false;
    }

    return PosTrans;
}

void njunaoModule::ContinuousFindBall()
{
    std::vector<float> PosTrans;
    PosTrans.push_back(0);
    PosTrans.push_back(0);
    if ((!Detecting)&&RefeshingFlag)
    {
        while (StartDetect)
        {
            Detecting=true;
         //   RefeshMat();
            std::vector<float> RedBallPosition=DetectRedBall(fIplImageHeader);
            
            PosTrans[0]=0;
            PosTrans[1]=0;
            if (!(RedBallPosition[0]==0&&RedBallPosition[1]==0&&RedBallPosition[2]==0))
            {
                if (!(imgWidth==0||imgHeight==0))
                {
                    PosTrans[0]=RedBallPosition[0]/(1.0f*imgWidth);
                    PosTrans[1]=RedBallPosition[1]/(1.0f*imgHeight);
                }
                
            }
            fMemProxy.insertData("njunaoBallPosition",PosTrans);
            fMemProxy.raiseEvent("njuFindBall",PosTrans);
            if (fMemProxy.getData("njunaoBallPositionStopFlag"))
            {
               // unRegisterFromVideoDevice();
                //exit();
                break;
            }
        }
        Detecting=false;
        
    }
    
}

std::vector<float> njunaoModule::PoleFind()
{
    std::vector<float> PolePos;
    
    PolePos.push_back(0);
    if (!PoleDetecting){
        PoleDetecting=true;
        if (StartDetect&&RefeshingFlag){
          //  RefeshMat();
            PolePos=DetectPole(fIplImageHeader);
            if (!(PolePos[0]==0))
            {
                PolePos[0]=PolePos[0]/(1.0f*imgWidth);
            }
            fMemProxy.insertData("njunaoPolePosition",PolePos);
            fMemProxy.raiseEvent("njuFindPole",PolePos);
        }
        PoleDetecting=false;
    }
    return PolePos;
}

void njunaoModule::ContinuousFindPole()
{
    std::vector<float> PolePos;
    PolePos.push_back(0);
    if (!PoleDetecting){
        PoleDetecting=true;
        while (StartDetect&&RefeshingFlag){
            PoleDetecting=true;
           // RefeshMat();
            PolePos=DetectPole(fIplImageHeader);
            if (!PolePos[0])
            {
                PolePos[0]=PolePos[0]/(1.0f*imgWidth);
            }
            fMemProxy.insertData("njunaoPolePosition",PolePos);
            fMemProxy.raiseEvent("njuFindPole",PolePos);
            if (fMemProxy.getData("njunaoPolePositionStopFlag"))
            {
                //unRegisterFromVideoDevice();
                //exit();
                break;
            }
        }
        PoleDetecting=false;
    }
}

void njunaoModule::exit()
{
    AL::ALModule::exit();
}


void njunaoModule::init()
{
    phraseToSay = "this version is 3.1";
    tts.post.say(phraseToSay);
    std::vector<float> PosTrans;
    std::vector<float> PolePos;
    
    PosTrans.push_back(0);
    PosTrans.push_back(0);
    PolePos.push_back(0);
    fMemProxy.insertData("njunaoBallPositionStopFlag",0);
    fMemProxy.insertData("njunaoBallPosition",PosTrans);
    fMemProxy.insertData("njunaoPolePositionStopFlag",0);
    fMemProxy.insertData("njunaoPolePosition",PolePos);
    fMemProxy.subscribeToEvent("njuFindBall","njunaoModule","njunaoBallPosition");
    fMemProxy.subscribeToEvent("njuFindPole","njunaoModule","njunaoPolePosition");
    try {
        fCamProxy = boost::shared_ptr<ALVideoDeviceProxy>(new ALVideoDeviceProxy(getParentBroker()));
        
    } catch (const AL::ALError& e) {
        qiLogError("vision.njunaomodule") << "Error while getting proxy on ALVideoDevice.  Error msg " << e.toString() << std::endl;
        njunaoModule::exit();
        return;
    }
    if(fCamProxy == NULL)
    {
        qiLogError("vision.njunaomodule") << "Error while getting proxy on ALVideoDevice. Check ALVideoDevice is running." << std::endl;
        njunaoModule::exit();
        return;
    }
    
    qiLogInfo("vision.njunaomodule") << "Use registerToVideoDevice + "
    "saveImageLocal + unRegisterFromVideoDevice to save images." << std::endl;
}

void njunaoModule::ContinuousRefreshCam()
{
    if (!RefeshingFlag){
    RefeshingFlag=true;
    while (RefeshingFlag)
    {
        RefeshMat();

        usleep(60000);
      //  if (expect_camera!=fCamProxy->getActiveCamera())
       // {
            //fCamProxy->unsubscribe(fVideoClientName);
        //    fCamProxy->setActiveCamera(expect_camera);
            //fVideoClientName = fCamProxy->subscribe(kOriginalName, Reso, pcs, 30 );
        //    usleep(500000);
       // }
    }
    RefeshingFlag=false;
    }
}

void njunaoModule::sayVersion()
{
    tts.post.say(phraseToSay);
}

void njunaoModule::ChangeCamera(int index)
{
    expect_camera=index;
}



