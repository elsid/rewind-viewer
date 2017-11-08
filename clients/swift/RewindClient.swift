/**
 *  Class for interaction with rewind-viewer from your own startegy class
 *  
 *  Implemented using CActiveSocket, which is shipped with swift-cgdk
 *  For each frame (game tick) rewind-viewer expect "end" command at frame end
 *  All objects should be represented as json string, 
 *  and will be decoded at viewer side to corresponding structures
 *  
 *  For available types see enum PrimitveType in <path to primitives here> 
 *
 *  In order to provide support for your own primitives:
 *     - add type with name of your choice to PrimitiveType in <path to primitives here> 
 *     - add structure with field description in file mentioned above 
 *       (you don't need to be expert in c++ to do that, just use already defined types as reference)
 *     - add function for desirialisation from json in same file (see defined types in <path to primitives here>)
 *     - send json string with your object instance description
 *  
 *  Note: All command only affect currently rendering frame and will not appear in next frame
 *  Currently you should send object each frame to display it in viewer
 */

#if ENABLE_REWIND_CLIENT

public class RewindClient {
  public static let instance = RewindClient(host: "192.168.1.2", port: 7000)!
  
  public enum Color: UInt32 {
    case red   = 0xFF0000
    case green = 0x00FF00
    case blue  = 0x0000FF
    case gray  = 0x273142
  }
  
  fileprivate let tc: TCPClient // просто для короткой запись, ибо писал в блокноте
  fileprivate var bytes: [Byte] = []
  
  ///Should be send on end of move function
  ///all turn primitives can be rendered after that point
  public func endFrame() {
    write(byte: Byte(bitPattern: 101/*e*/))
    
    tc.write(byte: Byte(bitPattern: 98/*b*/))
    tc.write(int: bytes.count)
    tc.write(bytes: bytes)
    bytes.removeAll()
  }
  
  public func circle(x: Double, y: Double, r: Double, color: Int32) {
    write(byte: Byte(bitPattern: 99/*c*/))
    write(float: Float(x))
    write(float: Float(y))
    write(float: Float(r))
    write(int32: color)
  }
  
  public func rect(x1: Double, y1: Double, x2: Double, y2: Double, color: Int32) {
    write(byte: Byte(bitPattern: 114/*r*/))
    write(float: Float(x1))
    write(float: Float(y1))
    write(float: Float(x2))
    write(float: Float(y2))
    write(int32: color)
  }
  
  public func line(x1: Double, y1: Double, x2: Double, y2: Double, color: Int32) {
    write(byte: Byte(bitPattern: 108/*l*/))
    write(float: Float(x1))
    write(float: Float(y1))
    write(float: Float(x2))
    write(float: Float(y2))
    write(int32: color)
  }
  
  ///Living unit - circle with HP bar
  ///x, y, r - same as for circle
  ///hp, max_hp - current life level and maximum level respectively
  ///enemy - 3 state variable: 1 - for enemy; -1 - for friend; 0 - neutral.
  ///course - parameter needed only to properly rotate textures (it unused by untextured units)
  ///unit_type - define used texture, value 0 means 'no texture'. For supported textures see enum UnitType in Frame.h
  public func livingUnit(x: Double, y: Double, r: Double, hp: Int, maxHP: Int, enemy: Int, course: Double = 0, utype: Int = 0) {
    write(byte: Byte(bitPattern: 117/*u*/))
    write(float: Float(x))
    write(float: Float(y))
    write(float: Float(r))
    write(int32: Int32(hp))
    write(int32: Int32(maxHP))
    write(short: Int16(utype))
    write(short: Int16(enemy))
    write(float: Float(course))
  }
  
  public func message(_ msg: String) {
    write(byte: Byte(bitPattern: 109/*m*/))
    
    let bytes = msg.utf8.map{ Byte(bitPattern: $0) }
    write(int32: Int32(bytes.count))
    self.bytes.append(contentsOf: bytes)
  }
  
  public func area(_ x: Int, y: Int, atype: Int) {
    write(byte: Byte(bitPattern: 97/*a*/))
    write(int32: Int32(x))
    write(int32: Int32(y))
    write(short: Int16(atype))
  }
  
  init?(host: String, port: Int) {
    tc = TCPClient(address: host, port: Int32(port))
    if !tc.connect() {
      print("Can't connect to host: \(host) port: \(port)")
      return nil
    }
  }
  
  private func write(byte value: Byte) {
    bytes.append(value)
  }
  
  private func write(float value: Float) {
    write(int32: fromByteArray(toByteArray(value)) as Int32)
  }
  
  private func write(int32 value: Int32) {
    bytes.append(contentsOf: toByteArray(value.littleEndian))
  }
  
  private func write(short value: Int16) {
    bytes.append(contentsOf: toByteArray(value.littleEndian))
  }
  
}


private func toByteArray<T>(_ value: T) -> [Byte] {
  var value = value
  return withUnsafeBytes(of: &value) {  $0.map{ Byte(bitPattern:$0) } }
}

private func fromByteArray<T>(_ value: [Byte]) -> T {
  return value.withUnsafeBytes {
    $0.baseAddress!.load(as: T.self)
  }
}

  
#endif
