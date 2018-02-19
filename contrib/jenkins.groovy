#!/usr/bin/env groovy

node('master') {
   deleteDir()
   checkout scm
   dir('swx_ci') {
      checkout([$class: 'GitSCM', 
              extensions: [[$class: 'CloneOption',  shallow: true]], 
              userRemoteConfigs: [[ url: 'https://github.com/Mellanox/swx_ci.git']]
            ])
      }
  evaluate(readFile("${env.WORKSPACE}/swx_ci/sockperf/test_jenkins_pipeline.groovy"))
}
