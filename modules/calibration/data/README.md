= Vehicle Calibration Configs =

For a well functioning vehicle, generally you need the following config files:

```text
<vehicle_dir>
  - vehicle_param.pb.txt      # Instance of apollo.common.VehicleParam
  - calibration_table.pb.txt  # Instance of apollo.control.ControlConf
  - velodyne_params           # Params for velodyne
      - 64E_S3_calibration_example.yaml
      - velodyne64_novatel_extrinsics_example.yaml
  - start_velodyne.launch     # Velodyne launch file for ROS.
  - gnss_params               # Params for GNSS
      - gnss_conf_mkz.txt     # GNSS config file
      - gnss_driver.launch    # GNSS launch file for ROS.
```
