import serial
from serial.tools import list_ports


def read_exact(ser, count):
    """Read exactly 'count' bytes or return None on timeout."""
    data = bytearray()
    while len(data) < count:
        chunk = ser.read(count - len(data))
        if not chunk:
            return None
        data.extend(chunk)
    return bytes(data)

class DataPacket:
    def __init__(self, packet_type, packet_length, hr, spo2, resp, temp):
        self.packet_type = packet_type
        self.packet_length = packet_length
        self.hr = hr
        self.spo2 = spo2
        self.resp = resp
        self.temp = temp

    def __str__(self):
        return f"DataPacket(type={self.packet_type}, length={self.packet_length}, hr={self.hr}, spo2={self.spo2}, resp={self.resp}, temp={self.temp})"

    @staticmethod
    def from_bytes(packet_bytes):
        if packet_bytes[0] != start_byte or packet_bytes[-1] != stop_byte:
            raise ValueError("Invalid packet format")
        packet_type = packet_bytes[4]
        #packet length in postiion 2 and 3
        packet_length = (packet_bytes[3] << 8) | packet_bytes[2]

        if packet_length != len(packet_bytes) - 5 - 2:  #exclude header and footer
            raise ValueError("Invalid packet length")

        offset = 5
        spo2 = packet_bytes[19+offset]
        hr = packet_bytes[20+offset]
        resp = packet_bytes[21+offset]
        temp = (packet_bytes[17+offset] | (packet_bytes[18+offset] << 8)) / 100.0
        return DataPacket(packet_type, packet_length, hr, spo2, resp, temp)

# display available serial ports
availaible_ports = list(list_ports.comports())
if not availaible_ports:
    print("No serial ports found.")
else:
    print("Available serial ports:")
    for port in availaible_ports:
        print(f" - {port.device}")

user_port = input("Enter the port to use: ")
# open the serial port
baudrate = 9600
ser = serial.Serial(user_port, baudrate, timeout=1)
print(f"Opened serial port: {ser.name}")

# read data from the serial port
start_byte = 0x0a
start_byte_2 = 0xFA
stop_byte = 0x0b

try:
    while True:
        first = ser.read(1)
        if not first or first[0] != start_byte:
            continue

        # Read the rest of the 5-byte header: [start2, len_lsb, len_msb, type]
        header_rest = read_exact(ser, 4)
        if header_rest is None:
            print("Timeout while reading packet header")
            continue

        if header_rest[0] != start_byte_2:
            print(f"Invalid second start byte: 0x{header_rest[0]:02X}")
            continue

        packet_length = (header_rest[2] << 8) | header_rest[1]

        # Read payload + 2-byte footer [0x00, stop]
        payload_and_footer = read_exact(ser, packet_length + 2)
        if payload_and_footer is None:
            print("Timeout while reading payload/footer")
            continue

        if payload_and_footer[-1] != stop_byte:
            print(f"Invalid stop byte: 0x{payload_and_footer[-1]:02X}")
            continue

        packet = bytearray()
        packet.extend(first)
        packet.extend(header_rest)
        packet.extend(payload_and_footer)

        try:
            print(DataPacket.from_bytes(packet))
        except ValueError as e:
            print(f"Error decoding packet: {e}")

except KeyboardInterrupt:
    print("Interrupted by user.")
    #close port
    ser.close()
    print(f"Closed serial port: {ser.name}")
