################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/Shell/CMD.cpp \
../src/Shell/ShellSession.cpp 

OBJS += \
./src/Shell/CMD.o \
./src/Shell/ShellSession.o 

CPP_DEPS += \
./src/Shell/CMD.d \
./src/Shell/ShellSession.d 


# Each subdirectory must supply rules for building sources it contributes
src/Shell/%.o: ../src/Shell/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	arm-linux-gnueabi-g++ -std=c++1y -I/home/xzl/soft -I"/Users/xzl/git/ZLMediaKit/src" -I../../ZLToolKit/src -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


