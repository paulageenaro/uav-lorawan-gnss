from(bucket: "lorawan-gps-uav")
  |> range(start: -3h)
  |> filter(fn: (r) => r["dev_eui"] == "a4cf123456789a01")
  |> filter(fn: (r) =>
    r["_measurement"] == "device_uplink" or
    r["_measurement"] =~ /^device_frmpayload_data/
  )
