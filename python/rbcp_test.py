from sitcpy.rbcp import Rbcp

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
        self.writereg(UDP_BASE_ADDR, 0x13, '01')
        self.writereg(UDP_BASE_ADDR, 0x13, '00')
        for i in range(32):
            self.writereg(UDP_BASE_ADDR, 0x01, '00')
            if code2[i] == 1:
                self.writereg(UDP_BASE_ADDR, 0x02, '01')
            else:
                self.writereg(UDP_BASE_ADDR, 0x02, '00')
            self.writereg(UDP_BASE_ADDR, 0x01, '01')
        self.writereg(UDP_BASE_ADDR, 0x01, '00')
        self.writereg(UDP_BASE_ADDR, 0x13, '01')
