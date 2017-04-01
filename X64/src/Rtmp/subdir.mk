################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/Rtmp/RtmpMediaSource.cpp \
../src/Rtmp/RtmpParser.cpp \
../src/Rtmp/RtmpPlayer.cpp \
../src/Rtmp/RtmpPlayerImp.cpp \
../src/Rtmp/RtmpProtocol.cpp \
../src/Rtmp/RtmpPusher.cpp \
../src/Rtmp/RtmpSession.cpp \
../src/Rtmp/RtmpToRtspMediaSource.cpp \
../src/Rtmp/amf.cpp \
../src/Rtmp/utils.cpp 

OBJS += \
./src/Rtmp/RtmpMediaSource.o \
./src/Rtmp/RtmpParser.o \
./src/Rtmp/RtmpPlayer.o \
./src/Rtmp/RtmpPlayerImp.o \
./src/Rtmp/RtmpProtocol.o \
./src/Rtmp/RtmpPusher.o \
./src/Rtmp/RtmpSession.o \
./src/Rtmp/RtmpToRtspMediaSource.o \
./src/Rtmp/amf.o \
./src/Rtmp/utils.o 

CPP_DEPS += \
./src/Rtmp/RtmpMediaSource.d \
./src/Rtmp/RtmpParser.d \
./src/Rtmp/RtmpPlayer.d \
./src/Rtmp/RtmpPlayerImp.d \
./src/Rtmp/RtmpProtocol.d \
./src/Rtmp/RtmpPusher.d \
./src/Rtmp/RtmpSession.d \
./src/Rtmp/RtmpToRtspMediaSource.d \
./src/Rtmp/amf.d \
./src/Rtmp/utils.d 


# Each subdirectory must supply rules for building sources it contributes
src/Rtmp/%.o: ../src/Rtmp/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -std=c++1y -DENABLE_FAAC -DENABLE_RTSP2RTMP -DENABLE_RTMP2RTSP -DENABLE_MEDIAFILE -DENABLE_X264 -I"/Users/xzl/git/ZLMediaKit/src" -I../../ZLToolKit/src -O3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


