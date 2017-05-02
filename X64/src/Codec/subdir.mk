################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/Codec/AACEncoder.cpp \
../src/Codec/H264Encoder.cpp 

OBJS += \
./src/Codec/AACEncoder.o \
./src/Codec/H264Encoder.o 

CPP_DEPS += \
./src/Codec/AACEncoder.d \
./src/Codec/H264Encoder.d 


# Each subdirectory must supply rules for building sources it contributes
src/Codec/%.o: ../src/Codec/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -std=c++1y -DENABLE_FAAC -DENABLE_RTSP2RTMP -DENABLE_RTMP2RTSP -DENABLE_MEDIAFILE -DENABLE_X264 -I"/home/xzl/git/ZLMediaKit/src" -I../../ZLToolKit/src -O3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


