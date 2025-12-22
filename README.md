
# 113-2 Car Car Class - Final Project

**Cornerstone EECS Design and Implementation** (often called the **“car car class”**) is a hands-on Information and Communication Technology (ICT) project course for freshmen. Before midterm, students build an Arduino-based line-following robot car and compete by developing algorithms to find the highest-scoring path. For the final project, students freely design and build a unique device using platforms such as Arduino, ESP32, or Raspberry Pi (RPi) with related modules—no domain restrictions.

For our final project, we built an **interactive wind-instrument practice device**. We moved the instrument’s key layout onto a pair of gloves, allowing users to practice fingerings on a tabletop like playing a piano. A head-mounted airflow sensor simulates breath control and volume, enabling practice anytime, anywhere without the actual instrument.

The device provides **three modes**: **Free Play**, **Fingering Practice**, and **Song Performance**. Fingering Practice guides users step-by-step with LED cues on the glove. Song Performance works like a rhythm game—the LEDs follow the song’s tempo, and users play along to perform the piece.

## Demo

## System Design



## Dataset

Download the datasets from [\_dataset_](https://drive.google.com/drive/folders/1bXjlnU0Q90I6w5YoS63aVochboCR5jVW?usp=drive_link) and unzip them.

## Pose Retrieval & Estimation

Follow the instruction of [ShuHong Chen et al.](https://github.com/ShuhongChen/bizarre-pose-estimator)
and merge the scripts in ```pose_retr_esti``` to _scripts.

## Transformation

The scipts in this folder contains all of the tranformation scripts we use.

* ```transform_img.ipynb``` is to apply swirl effect on character image.
* ```transform_sk.ipynb``` is to apply translation/rotation/scaling/resize-half/swirl effect on skeletons.
* For transformation required for deterministic method, follow the sequence provide in ```deterministic``` folder.

## Training

To train the model(s) in the paper, run this command:

```train
python train.py <root_dir> <save_path> <out_root> --use_stn true # for STN-integrated method
python train.py <root_dir> <save_path> <out_root> --use_stn false # for deterministic method
```
 * <root_dir>: path to dataset root
 * <save_path>: path to save checkpoints
 * <out_root>: path to save inference results

>📋  The inference will be done automatically right after training.
