from sitcpy.rbcp import Rbcp
from sitcpy.daq_client import DaqHandler
import time
import os
import numpy as np
import csv

SPI_BASE_ADDR = 0x00010000
UDP_BASE_ADDR = 0x00000000

class Gpio_reg:
    def __init__(self, ip_address="192.168.10.16"):
        self._rbcp = Rbcp(ip_address)

    def writereg(self, BASE_ADDR, address, value):
        self._rbcp.write(BASE_ADDR+address, bytearray.fromhex(value))
        try:
            self._rbcp.write(BASE_ADDR+address, bytearray.fromhex(value))
        except:
            try:
                self._rbcp.write(BASE_ADDR+address, bytearray.fromhex(value))
            except:
                self._rbcp.write(BASE_ADDR+address, bytearray.fromhex(value))

    def writespi(self, BASE_ADDR, address, value):
        b = hex(int('10' + address + '0' + value, 2))
        try:
            self._rbcp.write(BASE_ADDR + int(b[0:4], 16), bytearray.fromhex(b[4:6]))
        except:
            try:
                self._rbcp.write(BASE_ADDR + int(b[0:4], 16), bytearray.fromhex('00'))
            except:
                self._rbcp.write(BASE_ADDR + int(b[0:4], 16), bytearray.fromhex('00'))
                

    def readback(self, BASE_ADDR, address):
        self._rbcp.read(BASE_ADDR+address, 4)
        value = self._rbcp.read(BASE_ADDR, 4)
        return value

    def test_chip(self):
        self.writereg(UDP_BASE_ADDR, 0x05, '00')

    def dec2bin(self, string_num):
        base = [str(x) for x in range(10)] + [chr(x) for x in range(ord('A'), ord('A') + 6)]
        num = int(string_num)
        mid = []
        while True:
            if num == 0: break
            num, rem = divmod(num, 2)
            mid.append(base[rem])

        return ''.join([str(x) for x in mid[::-1]])

    def hex2dec(self,string_num):
        return str(int(string_num.upper(), 16))

    def hex2bin(self, string_num):
        return self.dec2bin(self.hex2dec(string_num.upper()))

    def set_dac_board(self,code):
        code1 = []

        t = self.hex2bin(code)
        for i in t:
            code1.append(int(i))
        code2 = code1[5:37]
        self.writereg(UDP_BASE_ADDR, 0x13, '01')          #FW DAC_NSYNC 1
        self.writereg(UDP_BASE_ADDR, 0x13, '00')          #FW DAC_NSYNC 0
        for i in range(32):
            self.writereg(UDP_BASE_ADDR, 0x01, '00')      #FW DAC_SCLK  0
            if code2[i] == 1:
                self.writereg(UDP_BASE_ADDR, 0x02, '01')  #FW DAC_DIN   1
            else:
                self.writereg(UDP_BASE_ADDR, 0x02, '00')  #FW DAC_DIN   0
            self.writereg(UDP_BASE_ADDR, 0x01, '01')      #FW DAC_SCLK  1
        self.writereg(UDP_BASE_ADDR, 0x01, '00')          #FW DAC_SCLK  0
        self.writereg(UDP_BASE_ADDR, 0x13, '01')          #FW DAC_NSYNC 1
        
def enter_to_register(noise_pixels_list, file_address, hit_number_threshold, open_rectangle_list, ITHR, column_level_list):
    array_mask = np.full((512, 1024), '1', dtype=str) #mask原始矩阵
    array_calen = np.full((512, 1024), '0', dtype=str) #calen原始矩阵
    column_level_array = np.full(512, '1', dtype=str) #列级屏蔽原始矩阵
    
    #开矩形窗口
    for i in range(1, int(len(open_rectangle_list)/4+1), 1): #python中range前开后闭，len(open_rectangle_list)/4为窗口数目，一般只开一个，open_rectangle_list由main程序传入
        for rec_i in range(open_rectangle_list[i*4-4], open_rectangle_list[i*4-3]+1, 1): #open_rectangle_list[i*4-4]为column1，open_rectangle_list[i*4-3]为column2
            for rec_j in range(open_rectangle_list[i*4-2], open_rectangle_list[i*4-1]+1, 1): #open_rectangle_list[i*4-2]为row1，open_rectangle_list[i*4-1]为row2
                array_mask[rec_i][rec_j] = "0"
                array_calen[rec_i][rec_j] = "1"                     
    
    #读取噪声像素文件，文件第一列为column，第二列为row，第三列为噪声出现次数    
    if(len(file_address) != 0):  #文件地址file_address由 main程序传入          
        csv_file = csv.reader(open(file_address, "r"))
        noise_excel = []
        for line in csv_file:
            noise_excel.append(line)#80-83行把csv文件读入在noise_excel数组里
        for i in range(0, len(noise_excel), 1):
            if(int(noise_excel[i][2]) > hit_number_threshold): #如果噪声出现次数大于hit_number_threshold的值，就屏蔽该像素, open_rectangle_list由main程序传入
                array_mask[int(noise_excel[i][0])][int(noise_excel[i][1])] = "1"
                array_calen[int(noise_excel[i][0])][int(noise_excel[i][1])] = "0" 
    
    #手动屏蔽单个像素    
    for i in range(1, int(len(noise_pixels_list)/2+1), 1): #len(noise_pixels_list)/2为噪声像素数目，一般只开一个，noise_pixels_list由main程序传入
        array_mask[noise_pixels_list[i*2-2]][noise_pixels_list[i*2-1]] = "1" #noise_pixels_list[i*2-2]为噪声像素的column， noise_pixels_list[i*2-1]为row
        array_calen[noise_pixels_list[i*2-2]][noise_pixels_list[i*2-1]] = "0"    
    
    array_mask = array_mask[::-1] #矩阵0-511反转
    array_calen = array_calen[::-1]  
    #71-95行各个模块有优先级，屏蔽噪声像素一定要放在开矩形窗口后面
    
    gpio_reg = Gpio_reg('192.168.10.16')
    gpio_reg.test_chip
    gpio_reg.writereg(UDP_BASE_ADDR, 0x03, '01')  #FW CHIP_RESTN_CLEAR  1
    gpio_reg.writereg(UDP_BASE_ADDR, 0x04, '01')  #FW CHIP_RESTN_SET    1
    gpio_reg.writereg(UDP_BASE_ADDR, 0x03, '00')  #FW CHIP_RESTN_CLEAR  0
    gpio_reg.writereg(UDP_BASE_ADDR, 0x04, '00')  #FW CHIP_RESTN_SET    0

    gpio_reg.writespi(SPI_BASE_ADDR, '10110', '00000010')  #SN  BSEL 0 ISEL1 0 ISEL0 0 EXCKS 0 DSEL 0 CKESEL 0 RCKI (1) RCKO 0    
    gpio_reg.writespi(SPI_BASE_ADDR, '00001', '11010000')  #SN  TRIGN (1) CPRN (1) DOFREQ 01(d) SMOD 0 CTM 0 SPI_D 0 TMOD 0  
    gpio_reg.writespi(SPI_BASE_ADDR, '10111', '01000000')  #SN  DAC_REG114 0 DAC_REG113 1(d) DAC_REG112 0 LDO_REG1 0 LDO_REG0 0 C_MASK_EN 0 ENTP 0 EN10B 0  
    ####LSB=1 CML output
    gpio_reg.writespi(SPI_BASE_ADDR, '11110', '00000100')  #SN  RESERVED13N7_4 0000 RESERVED13N3 0 PSET (1) OISEL 0 OPSEL 0 
    gpio_reg.writereg(UDP_BASE_ADDR, 0x14, '04')  #FW SER_DELAY        0x04
    gpio_reg.writereg(UDP_BASE_ADDR, 0x15, 'ff')  #FW FW_SOFT_RESET    0xff

    for j in range(0, 1024, 1):
        if(j!=0):
            bit_flag_1 = bit_flag_2
        bit_flag_2 = ''
        for i in range(0, 64, 1): 
            for bit_i in range(i*8, i*8+8, 1):
                bit_flag_2 = bit_flag_2+array_mask[bit_i][j]
        if(j==0): #第一行直接配置
            for i in range(0, 64, 1):        
                mask_eight_bit = ''
                for bit_i in range(i*8, i*8+8, 1):
                    mask_eight_bit = mask_eight_bit+(array_mask[bit_i][j])             
                gpio_reg.writespi(SPI_BASE_ADDR, '00110', mask_eight_bit)   #SN PIXELMASK_DATA   
        elif(bit_flag_2!= bit_flag_1): #和上一行不同则配置
            for i in range(0, 64, 1):        
                mask_eight_bit = ''
                for bit_i in range(i*8, i*8+8, 1):
                    mask_eight_bit = mask_eight_bit+(array_mask[bit_i][j])  
                gpio_reg.writespi(SPI_BASE_ADDR, '00110', mask_eight_bit)   #SN PIXELMASK_DATA      
        gpio_reg.writespi(SPI_BASE_ADDR, '00111', '00000000')               #SN LOADC_E 0 LOADM_E 0  000000

    gpio_reg.writereg(UDP_BASE_ADDR, 0x0b, '00')    #FW LOAD_M 0 
    gpio_reg.writereg(UDP_BASE_ADDR, 0x0b, '01')    #FW LOAD_M 1
    gpio_reg.writereg(UDP_BASE_ADDR, 0x0b, '00')    #FW LOAD_M 0

    for j in range(0, 1024, 1):
        if(j!=0):
            bit_flag_1 = bit_flag_2
        bit_flag_2 = ''
        for i in range(0, 64, 1):
            for bit_i in range(i*8, i*8+8, 1):
                bit_flag_2 = bit_flag_2+array_calen[bit_i][j]
        if(j==0): #第一行直接配置
            for i in range(0, 64, 1):
                calen_eight_bit = ''
                for bit_i in range(i*8, i*8+8, 1):
                    calen_eight_bit = calen_eight_bit+(array_calen[bit_i][j])    # bit order flip
                gpio_reg.writespi(SPI_BASE_ADDR, '00110', calen_eight_bit) 
        elif(bit_flag_2!= bit_flag_1): #和上一行不同则配置 
            for i in range(0, 64, 1):
                calen_eight_bit = ''
                for bit_i in range(i*8, i*8+8, 1):
                    calen_eight_bit = calen_eight_bit+(array_calen[bit_i][j]) 
                gpio_reg.writespi(SPI_BASE_ADDR, '00110', calen_eight_bit)         
        gpio_reg.writespi(SPI_BASE_ADDR, '00111', '00000000') 			

    gpio_reg.writereg(UDP_BASE_ADDR, 0x0c, '00')  #FW LOAD_C 0
    gpio_reg.writereg(UDP_BASE_ADDR, 0x0c, '01')  #FW LOAD_C 1
    gpio_reg.writereg(UDP_BASE_ADDR, 0x0c, '00')  #FW LOAD_C 0
    
    #列级屏蔽，一次屏蔽一列
    if(len(column_level_list) != 0):
        for i in range(0, int(len(column_level_list)), 1):
            column_level_array[column_level_list[i]] = "0"
        column_level_array = column_level_array[::-1]
        for i in range(0, 64, 1):
            column_mask_bit = ''
            for bit_i in range(i*8, i*8+8, 1):
                column_mask_bit = column_mask_bit+(column_level_array[bit_i])              
            gpio_reg.writespi(SPI_BASE_ADDR, '00110', column_mask_bit)   
        gpio_reg.writespi(SPI_BASE_ADDR, '10111', '01000100')    #SN DAC_REG114 0 DAC_REG113 1(d) DAC_REG112 0 LDO_REG1 0 LDO_REG0 0 C_MASK_EN (1) ENTP 0 EN10B 0 
                    

    # set_dac_board
    gpio_reg.set_dac_board('10021a3d60')
    gpio_reg.set_dac_board('1002030210')
    gpio_reg.set_dac_board('10022a9150')
    #VR = 1.405V
	
    ##### set_dac_chip
    gpio_reg.writespi(SPI_BASE_ADDR, '01000', '01000101')   #SN DAC_REG7_0   01000101 
    gpio_reg.writespi(SPI_BASE_ADDR, '01001', '01100000')   #SN DAC_REG15_8  01100000
    
    #计算ITHR的二进制并配置给寄存器，输入时只能给偶数
    ITHR_two = bin(ITHR)
    ITHR_eight = ITHR_two[2:len(ITHR_two)-1:1]
    for i in range(1,8-len(ITHR_eight),1):
        ITHR_eight = '0' + ITHR_eight
    ITHR_eight = '1' + ITHR_eight
    print(ITHR_eight)
    gpio_reg.writespi(SPI_BASE_ADDR, '01010', ITHR_eight)   #SN  DAC_REG23_16 
    
    ##ITHR=100000--> REG[22:15]
    ##ITHR=11000--> REG[22:15]
    gpio_reg.writespi(SPI_BASE_ADDR, '01011', '00001011')   #SN DAC_REG31_24   00001011 
    gpio_reg.writespi(SPI_BASE_ADDR, '01100', '00000110')   #SN DAC_REG39_32   00000110
    gpio_reg.writespi(SPI_BASE_ADDR, '01101', '00000000')   #SN DAC_REG47_40   00000000 
    gpio_reg.writespi(SPI_BASE_ADDR, '01110', '11011110')   #SN DAC_REG55_48   11011110 
    gpio_reg.writespi(SPI_BASE_ADDR, '01111', '00000000')   #SN DAC_REG63_56   00000000 

    #VCASP=10101011111--> REG[62:53]
    gpio_reg.writespi(SPI_BASE_ADDR, '10000', '11111000')   #SN DAC_REG71_64   11111000 
    gpio_reg.writespi(SPI_BASE_ADDR, '10001', '10010101')   #SN DAC_REG79_72   10010101
    gpio_reg.writespi(SPI_BASE_ADDR, '10010', '11100000')   #SN DAC_REG87_80   11100000
    gpio_reg.writespi(SPI_BASE_ADDR, '10011', '10001001')   #SN DAC_REG95_88   10001001
    gpio_reg.writespi(SPI_BASE_ADDR, '10100', '10000010')   #SN DAC_REG103_96  10000010
    gpio_reg.writespi(SPI_BASE_ADDR, '10101', '01010111')   #SN DAC_REG111_104 01010111
    gpio_reg.writespi(SPI_BASE_ADDR, '11100', '00010010')   #SN REG_CDAC_8NA4_0 (00010)  EN_CDAC_8NA (0) REG_CDAC_8NA_BGR 1 REG_SEL_CDAC_8NA (0) 
    gpio_reg.writespi(SPI_BASE_ADDR, '11101', '00000000')   #SN 00000 REG_CDAC_8NA_TRIM 00 REG_CDAC_8NA5 0 

def welcome_interface():
    print(" ")
    print("**************************************************")
    print("Hi, welcome to Taichu3 configuration interface.")
    print("Wish you have a good time!")
    print("**************************************************")

#手动屏蔽单个像素时输入行和列，并返回给main函数
#flag后缀的变量用于防止输入不合理的数，导致程序错误    
def add_noise_pixel():
    print(" ")
    print("Add a noise pixel:")
    noise_pixel_column_flag = 1
    noise_pixel_row_flag = 1
    
    while noise_pixel_column_flag:
        noise_pixel_column = int(input("Please enter column number:"))
        if((noise_pixel_column >= 0) and (noise_pixel_column < 512)):
            noise_pixel_column_flag = 0
        else:
            print("You entered wrong number.")
            print("Please enter again.")
            
    while noise_pixel_row_flag:
        noise_pixel_row = int(input("Please enter row number:"))
        if((noise_pixel_row >= 0) and (noise_pixel_row < 1024)):
            noise_pixel_row_flag = 0
        else:
            print("You entered wrong number.")
            print("Please enter again.")
            
    return[noise_pixel_column, noise_pixel_row]
    
#删除手动屏蔽的单个像素    
def delete_noise_pixel(noise_list):
    print(" ")
    print("Delete a noise pixel:")    
    delete_pixel_flag = 1
    
    while delete_pixel_flag:
        del_number = 2*int(input("Which noise pixel do you want to delete:"))-2
        if((del_number >= 0) and (del_number <= (len(noise_list)-2))):
            delete_pixel_flag = 0
        else:
            print("You entered wrong number.")
            print("Please enter again.")
            
    return del_number

#输入噪声像素文件的地址并返回给main函数   
def read_excel():
    print(" ")
    print("Read noise pixels excel:")
    file_address = input("Please enter file address:")
    return file_address

#改变hit_number_threshold的值并返回给main函数
def change_hit_number_threshold():
    print(" ")
    print("Change noise pixels excel hit number threshold:")
    change_threshold_flag = 1
    
    while change_threshold_flag:
        hit_number_threshold = int(input("Please enter hit number threshold:"))
        if(hit_number_threshold >= 0):
            change_threshold_flag = 0
        else:
            print("You entered wrong number.")
            print("Please enter again.")
            
    return hit_number_threshold

#输入开矩形窗口的四个边界参数并返回给main函数
def add_open_rectangle():
    print(" ")
    print("Add a open rectangle:")
    open_rectangle_column1_flag = 1
    open_rectangle_column2_flag = 1
    open_rectangle_row1_flag = 1
    open_rectangle_row2_flag = 1
    
    while open_rectangle_column1_flag:
        open_rectangle_column1 = int(input("Please enter column number 1:"))
        if((open_rectangle_column1 >= 0) and (open_rectangle_column1 < 512)):
            open_rectangle_column1_flag = 0
        else:
            print("You entered wrong number.")
            print("Please enter again.")
            
    while open_rectangle_column2_flag:
        open_rectangle_column2 = int(input("Please enter column number 2:"))
        if((open_rectangle_column2 >= open_rectangle_column1) and (open_rectangle_column2 < 512)):
            open_rectangle_column2_flag = 0
        else:
            print("You entered wrong number.")
            print("Please enter again.")
            
    while open_rectangle_row1_flag:
        open_rectangle_row1 = int(input("Please enter row number 1:"))
        if((open_rectangle_row1 >= 0) and (open_rectangle_row1 < 1024)):
            open_rectangle_row1_flag = 0
        else:
            print("You entered wrong number.")
            print("Please enter again.")
            
    while open_rectangle_row2_flag:
        open_rectangle_row2 = int(input("Please enter row number 2:"))
        if((open_rectangle_row2 >= open_rectangle_row1) and (open_rectangle_row2 < 1024)):
            open_rectangle_row2_flag = 0
        else:
            print("You entered wrong number.")
            print("Please enter again.")
            
    return[open_rectangle_column1, open_rectangle_column2, open_rectangle_row1, open_rectangle_row2]

#删除某个矩形窗口     
def delete_open_rectangle(rec_list):
    print(" ")
    print("Delete a open rectangle:")    
    delete_pixel_flag = 1
    
    while delete_pixel_flag:
        del_number = 4*int(input("Which open rectangle do you want to delete:"))-4
        if((del_number >= 0) and (del_number <= (len(rec_list)-4))):
            delete_pixel_flag = 0
        else:
            print("You entered wrong number.")
            print("Please enter again.")
            
    return del_number
       
def change_ITHR():
    print(" ")
    print("Change ITHR:")
    change_ITHR_flag = 1
    
    while change_ITHR_flag:
        ITHR = int(input("Please enter ITHR value:"))
        if((ITHR >= 0) and (ITHR <= 128) and (ITHR%2 == 0)):
            change_ITHR_flag = 0
        else:
            print("You entered wrong number.")
            print("Please enter again.")
            
    return ITHR

#添加列级屏蔽的column并返回给main函数   
def add_column_level_mask():
    print(" ")
    print("Add column level mask:")
    column_level_mask_flag = 1
    
    while column_level_mask_flag:
        column_level_mask = int(input("Please enter column:"))
        if((column_level_mask >= 0) and (column_level_mask < 512)):
            column_level_mask_flag = 0
        else:
            print("You entered wrong number.")
            print("Please enter again.")
            
    return [column_level_mask]

#删除某个列级屏蔽    
def delete_column_level_mask(column_level_list):
    print(" ")
    print("Delete column level mask:")    
    delete_column_level_mask_flag = 1
    
    while delete_column_level_mask_flag:
        del_number = int(input("Which noise pixel do you want to delete:"))-1
        if((del_number >= 0) and (del_number <= (len(column_level_list)-1))):
            delete_column_level_mask_flag = 0
        else:
            print("You entered wrong number.")
            print("Please enter again.")
            
    return del_number

#显示当前的配置状态
def configuration_now(noise_pixels_list, file_address, hit_number_threshold, open_rectangle_list, ITHR, column_level_list):
    print(" ")
    print("--------------------------------------------------")
    print("Now the configuration is:")
    
    if(len(noise_pixels_list) != 0):
        print(" ")
        print("---Noise pixels:")
        for i in range(1, int(len(noise_pixels_list)/2+1), 1):
            print("noise pixel", i, ":", noise_pixels_list[i*2-2], ",", noise_pixels_list[i*2-1])
            
    if(len(file_address) != 0):
        print(" ")
        print("---Noise pixels excel address:", file_address)
        print("---Noise pixels excel hit number threshold:", hit_number_threshold)
    
    if(len(open_rectangle_list) != 0):
        print(" ")    
        print("---Open rectangles:")
        for i in range(1, int(len(open_rectangle_list)/4+1), 1):
            print("open rectangle", i, ":",open_rectangle_list[i*4-4], ",", open_rectangle_list[i*4-3], ",", open_rectangle_list[i*4-2], ",", open_rectangle_list[i*4-1])
        
    if(len(column_level_list) != 0):
        print(" ")
        print("---column level mask:")
        for i in range(1, int(len(column_level_list)+1), 1):
            print("column", i, ":", column_level_list[i-1])    
            
    print(" ")    
    print("---ITHR:", ITHR)
    print("--------------------------------------------------")
    print(" ")
    print("--------------------------------------------------")
    print("You have following options:")
    print("a.Enter this configuration into register")
    print(" ")
    print("b.Add a noise pixel")
    print("c.Delete a noise pixel")
    print(" ")
    print("d.Read noise pixels excel")
    print("e.Delete noise pixels excel")
    print("f.Change noise pixels excel hit number threshold")
    print(" ")
    print("g.Add an open rectangle")
    print("h.Delete an open rectangle")
    print(" ")
    print("k.Change ITHR")
    print(" ")
    print("l.Add column level mask")
    print("m.Delete column level mask")
    print(" ")
    print("n.Finish work")
    print("--------------------------------------------------")
       
def want_to_do():
    print(" ")
    return input("Which of above do you want:")

#主程序，选择要进行的操作
def main():    
    continue_work = 1 #continue_work=0时退出程序界面
    noise_pixels_list = []
    file_address = ""
    hit_number_threshold = 100 #默认值
    open_rectangle_list = []
    ITHR = 32 #默认值
    column_level_list = []

    welcome_interface()
    while continue_work:
        configuration_now(noise_pixels_list, file_address, hit_number_threshold, open_rectangle_list, ITHR, column_level_list)
        letter_flag = 1
        while letter_flag:
            want = want_to_do()
            
            if(want == 'a'): #执行配置程序
                enter_to_register(noise_pixels_list, file_address, hit_number_threshold, open_rectangle_list, ITHR, column_level_list) 
                print(" ")
                print("Successfully configured!")
                letter_flag = 0                
                
            elif(want == 'b'): #添加要手动屏蔽的单个像素，将行列值存入noise_pixels_list数组
                noise_pixels_list.extend(add_noise_pixel())
                letter_flag = 0
                
            elif(want == 'c'): #删除要手动屏蔽的单个像素，从noise_pixels_list数组中删除
                if(len(noise_pixels_list)==0):
                    print("There is nothing to delete.")
                else:                    
                    delete_number = delete_noise_pixel(noise_pixels_list)
                    del noise_pixels_list[delete_number]
                    del noise_pixels_list[delete_number]
                    letter_flag = 0
            
            elif(want == 'd'): #读取噪声像素文件地址
                file_address = read_excel()
                letter_flag = 0
            
            elif(want == 'e'): #删除噪声像素文件地址
                file_address = ""
                letter_flag = 0
                
            elif(want == 'f'): #改变hit_number_threshold的值
                hit_number_threshold = change_hit_number_threshold()
                letter_flag = 0
                
            elif(want == 'g'): #添加矩形窗口，将四个边界参数存入open_rectangle_list数组
                open_rectangle_list.extend(add_open_rectangle())
                letter_flag = 0
             
            elif(want == 'h'): #删除某个矩形窗口，从open_rectangle_list数组中删除
                if(len(open_rectangle_list)==0):
                    print("There is nothing to delete.")
                else:
                    delete_number = delete_open_rectangle(open_rectangle_list)
                    del open_rectangle_list[delete_number]
                    del open_rectangle_list[delete_number]
                    del open_rectangle_list[delete_number]
                    del open_rectangle_list[delete_number]
                    letter_flag = 0
                
            elif(want == 'k'):
                ITHR = change_ITHR()
                letter_flag = 0
            
            elif(want == 'l'): #添加列级屏蔽，将要屏蔽的列存入column_level_list数组
                column_level_list.extend(add_column_level_mask())
                letter_flag = 0
            
            elif(want == 'm'): #删除某个列级屏蔽，从column_level_list数组中删除
                if(len(column_level_list)==0):
                    print("There is nothing to delete.")
                else:                    
                    delete_number = delete_column_level_mask(column_level_list)
                    del column_level_list[delete_number]
                    letter_flag = 0
                    
            elif(want == 'n'): #结束工作
                print(" ")
                print("Bye!")
                continue_work = 0
                letter_flag = 0
                    
            else:
                print("You entered wrong letter.")
                print("Please enter again.")
            
if __name__ == "__main__":
    main()
        
