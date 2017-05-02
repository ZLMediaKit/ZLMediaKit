################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/Rtsp/RtpBroadCaster.cpp \
../src/Rtsp/RtpParser.cpp \
../src/Rtsp/Rtsp.cpp \
../src/Rtsp/RtspMediaSource.cpp \
../src/Rtsp/RtspPlayer.cpp \
../src/Rtsp/RtspPlayerImp.cpp \
../src/Rtsp/RtspSession.cpp \
../src/Rtsp/RtspToRtmpMediaSource.cpp \
../src/Rtsp/UDPServer.cpp 

OBJS += \
./src/Rtsp/RtpBroadCaster.o \
./src/Rtsp/RtpParser.o \
./src/Rtsp/Rtsp.o \
./src/Rtsp/RtspMediaSource.o \
./src/Rtsp/RtspPlayer.o \
./src/Rtsp/RtspPlayerImp.o \
./src/Rtsp/RtspSession.o \
./src/Rtsp/RtspToRtmpMediaSource.o \
./src/Rtsp/UDPServer.o 

CPP_DEPS += \
./src/Rtsp/RtpBroadCaster.d \
./src/Rtsp/RtpParser.d \
./src/Rtsp/Rtsp.d \
./src/Rtsp/RtspMediaSource.d \
./src/Rtsp/RtspPlayer.d \
./src/Rtsp/RtspPlayerImp.d \
./src/Rtsp/RtspSession.d \
./src/Rtsp/RtspToRtmpMediaSource.d \
./src/Rtsp/UDPServer.d 


# Each subdirectory must supply rules for building sources it contributes
src/Rtsp/%.o: ../src/Rtsp/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -std=c++1y -DENABLE_RTSP2RTMP -DENABLE_RTMP2RTSP -DENABLE_HLS -DENABLE_MP4V2 -DENABLE_FAAC -DENABLE_X264 -I"/home/xzl/git/ZLMediaKit/src" -I../../ZLToolKit/src -O3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


