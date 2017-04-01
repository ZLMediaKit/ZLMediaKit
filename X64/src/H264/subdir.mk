################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/H264/SPSParser.c 

CPP_SRCS += \
../src/H264/H264Parser.cpp \
../src/H264/h264_bit_reader.cpp \
../src/H264/h264_parser.cpp \
../src/H264/h264_poc.cpp \
../src/H264/ranges.cpp 

OBJS += \
./src/H264/H264Parser.o \
./src/H264/SPSParser.o \
./src/H264/h264_bit_reader.o \
./src/H264/h264_parser.o \
./src/H264/h264_poc.o \
./src/H264/ranges.o 

C_DEPS += \
./src/H264/SPSParser.d 

CPP_DEPS += \
./src/H264/H264Parser.d \
./src/H264/h264_bit_reader.d \
./src/H264/h264_parser.d \
./src/H264/h264_poc.d \
./src/H264/ranges.d 


# Each subdirectory must supply rules for building sources it contributes
src/H264/%.o: ../src/H264/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -std=c++1y -DENABLE_FAAC -DENABLE_RTSP2RTMP -DENABLE_RTMP2RTSP -DENABLE_MEDIAFILE -DENABLE_X264 -I"/Users/xzl/git/ZLMediaKit/src" -I../../ZLToolKit/src -O3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

src/H264/%.o: ../src/H264/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -O3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


