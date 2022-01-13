# Continuous Integration

## Retrigger

Put a comment with "bot:retest"

## More details

After create Pull Request CI runs automatically within 5 mins.
The CI results can be found under link Details on PR page in github.

There are two main sections: Build Artifacts and Blue Ocean.

* Build Artifacts has log files with output each stage.
* On Blue Ocean page you can see a graph with CI steps. Each stage can be success(green) or fail(red).

## CI/CD

This CD/CD  pipeline based on ci-demo. All information can be found in main ci-demo repository.
This is Jenkins CI/CD based project. You can create custom workflows to automate your project software life cycle process.

You need to configure workflows using YAML syntax, and save them as workflow files in your repository.
Once you've successfully created a YAML workflow file and triggered the workflow - Jenkins parse flow and execute it.

[Read more](https://github.com/Mellanox/ci-demo/blob/master/README.md)

## Update Jenkins

If proj_jjb.yaml was changed pipline configuration should be updated

```
   cd .ci
   make jjb
```

## Job Matrix yaml

The main CI steps are describe in `job_matrix.yaml` file.
Full list of supported features and syntax is given below.
For more information please refer to [official ci-demo repository](https://github.com/Mellanox/ci-demo/)

## How to run Docker image on local machine
 1. Make sure you have docker engine installed: `docker --version`
 2. If there is no docker, please install it this way ):
    ```sh
    apt update && apt install -y apt-transport-https ca-certificates curl software-properties-common
    curl -sSL https://get.docker.com/ | sh
    ```
 3. Pull and run container by the following command (use correct image for your case):
    ```sh
    # Find top level dir of the git project:
    WORKSPACE=$(git rev-parse --show-toplevel)

    # run ubuntu2004 image on x86_64:
    docker run -it -d --rm --privileged -e WORKSPACE=$WORKSPACE -v $WORKSPACE:$WORKSPACE -v /var/lib/hugetlbfs:/var/lib/hugetlbfs --ulimit memlock=819200000:819200000 -v /auto/mtrswgwork:/auto/mtrswgwork -v /hpc/local/commercial:/hpc/local/commercial -v /auto/sw_tools/Commercial:/auto/sw_tools/Commercial -v /hpc/local/etc/modulefiles:/hpc/local/etc/modulefiles harbor.mellanox.com/swx-infra/x86_64/ubuntu20.04/builder:mofed-5.2-2.2.0.0 bash

    # run toolbox image on x86_64:
    docker run -it -d --rm --privileged -e WORKSPACE=$WORKSPACE -v $WORKSPACE:$WORKSPACE -v /var/lib/hugetlbfs:/var/lib/hugetlbfs --ulimit memlock=819200000:819200000 -v /auto/mtrswgwork:/auto/mtrswgwork -v /hpc/local/commercial:/hpc/local/commercial -v /auto/sw_tools/Commercial:/auto/sw_tools/Commercial -v /hpc/local/etc/modulefiles:/hpc/local/etc/modulefiles harbor.mellanox.com/toolbox/ngci-centos:7.9.2009.2 bash
    ```
 4. Login to running docker image:
    ```sh
    docker exec -it ${IMAGE_NAME} bash
    ```
Navigate to `$WORKSPACE` directory inside the image

