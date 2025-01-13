# ID Generator
Cấu trúc của ID Generator:
+ Bit 1: Không sử dụng (bit sign).
+ Bits 2-41: Thời gian tính bằng milliseconds kể từ một thời điểm gốc (epoch).
+ Bits 42-51: ID máy (datacenter ID và worker ID).
+ Bits 52-63: Số tăng dần (sequence) để đảm bảo tính duy nhất nếu nhiều ID được tạo cùng một lúc.

Test API:
+ Endpoint `/generate-id`: Tạo một ID duy nhất và trả về dưới dạng JSON.
+ Endpoint `/generate-ids`: Tạo một số lượng ID nhất định (trong ví dụ là 10) và trả về danh sách các ID đó.



