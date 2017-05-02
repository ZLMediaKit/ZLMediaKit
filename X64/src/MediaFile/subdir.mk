################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/MediaFile/HLSMaker.cpp \
../src/MediaFile/MediaReader.cpp \
../src/MediaFile/MediaRecorder.cpp \
../src/MediaFile/Mp4Maker.cpp \
../src/MediaFile/TSMaker.cpp \
../src/MediaFile/crc32.cpp 

OBJS += \
./src/MediaFile/HLSMaker.o \
./src/MediaFile/MediaReader.o \
./src/MediaFile/MediaRecorder.o \
./src/MediaFile/Mp4Maker.o \
./src/MediaFile/TSMaker.o \
./src/MediaFile/crc32.o 

CPP_DEPS += \
./src/MediaFile/HLSMaker.d \
./src/MediaFile/MediaReader.d \
./src/MediaFile/MediaRecorder.d \
./src/MediaFile/Mp4Maker.d \
./src/MediaFile/TSMaker.d \
./src/MediaFile/crc32.d 


# Each subdirectory must supply rules for building sources it contributes
src/MediaFile/%.o: ../src/MediaFile/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -std=c++1y -DENABLE_RTSP2RTMP -DENABLE_RTMP2RTSP -DENABLE_HLS -DENABLE_MP4V2 -DENABLE_FAAC -DENABLE_X264 -I"/home/xzl/git/ZLMediaKit/src" -I../../ZLToolKit/src -O3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


