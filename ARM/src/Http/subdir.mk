################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/Http/HttpSession.cpp \
../src/Http/strCoding.cpp 

OBJS += \
./src/Http/HttpSession.o \
./src/Http/strCoding.o 

CPP_DEPS += \
./src/Http/HttpSession.d \
./src/Http/strCoding.d 


# Each subdirectory must supply rules for building sources it contributes
src/Http/%.o: ../src/Http/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	arm-linux-gnueabi-g++ -std=c++1y -I/home/xzl/soft -I"/Users/xzl/git/ZLMediaKit/src" -I../../ZLToolKit/src -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


