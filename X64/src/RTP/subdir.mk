################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/RTP/RtpMakerAAC.cpp \
../src/RTP/RtpMakerH264.cpp 

OBJS += \
./src/RTP/RtpMakerAAC.o \
./src/RTP/RtpMakerH264.o 

CPP_DEPS += \
./src/RTP/RtpMakerAAC.d \
./src/RTP/RtpMakerH264.d 


# Each subdirectory must supply rules for building sources it contributes
src/RTP/%.o: ../src/RTP/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -std=c++1y -DENABLE_FAAC -DENABLE_RTSP2RTMP -DENABLE_RTMP2RTSP -DENABLE_MEDIAFILE -DENABLE_X264 -I"/home/xzl/git/ZLMediaKit/src" -I../../ZLToolKit/src -O3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


