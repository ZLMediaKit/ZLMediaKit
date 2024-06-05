pluginManagement {
    repositories {
        mavenCentral()
        gradlePluginPortal()
        google()
    }
}
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        mavenCentral()
        google()
        maven {
            credentials {
                username = ("6256cd6c7e8dbc28d896a661")
                password = ("KRuEgA3WYUVy")
            }
            url = uri("https://packages.aliyun.com/maven/repository/2302596-release-mpvXBR/")
        }
    }
}

rootProject.name = "RTCPlayer"
include(":app")
//include(":RTCPlayer")
