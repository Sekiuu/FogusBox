import type { DeviceTimeSetting } from '~/types/mqtt'
export type TimeSettingFrom = {
  work_time: {
    minute: number;
    sec: number;
  };
  break_time: {
    minute: number;
    sec: number;
  };
}
export const useDeviceSettings = (connection: {
  settingTopic: { value: string }
  publishSettings: (payload: unknown) => Promise<void>
}) => {
  const convertToSec = (minute: number) => minute * 60

  const validate = (state : TimeSettingFrom): boolean => {
    const work = convertToSec(state.work_time.minute) + state.work_time.sec
    const break_ = convertToSec(state.break_time.minute) + state.break_time.sec
    return work > 0 && break_ > 0
  }

  const save = async (state : TimeSettingFrom): Promise<boolean> => {
    if (!validate(state)) return false
    console.log("SAVE SETTING")
    const payload: DeviceTimeSetting = {
      work_time: convertToSec(state.work_time.minute) + state.work_time.sec,
      break_time: convertToSec(state.break_time.minute) + state.break_time.sec,
    }

    try {
      await connection.publishSettings(payload)
      return true
    } catch (e) {
      console.error('publishSettings failed:', e)
      return false
    }
  }

  return {
    save,
  }
}
