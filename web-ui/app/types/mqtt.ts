/** Typed MQTT payloads used across the app */

import { MqttClient } from "mqtt";

export interface MqttPairingMessage {
  code: string
  device_id: string
}
export interface UseMQTT {
  subscribe: (topic: string, callback: (msg: string, topic: string) => void) => {
    unsubscribe: () => void;
  };
  publish: (topic: string, message: string) => void;
  waitForMassage: (topic: string, timeoutMs?: number) => Promise<string>;
  client: MqttClient;
}
export interface MqttStatusMessage {
  status?: string,
  state?: string,
  settings?: {
    work: number,
    break: number
  }
  runtime?: {
    time_remain?: number
    status?: string
    remain_time?: number
  }
  [key: string]: unknown
}

export interface DeviceTimeSetting {
  work_time: number
  break_time: number
}
