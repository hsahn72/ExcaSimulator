# 물리 인터페이스와 Chrono 연동 약속

굴착기의 **물리 계산**과 **화면에 보이는 부분**을 `IExcavatorPhysics`(`Source/ExcaSimulator/ExcavatorPhysics.h`) 하나로 나눴습니다. 물리 쪽 구현을 바꿔도 입력, VR, 지형, 렌더링 코드는 그대로 둘 수 있습니다.

```
 조이스틱 ──► AExcavatorPawn ──FExcavatorCommand──►  IExcavatorPhysics
                 │      ▲      ──FBucketSoilProbe──►  (단순 모델 / 유압 모델 / Chrono)
                 │      └──── FExcavatorPhysicsState ◄─┘
                 ▼
            ADigTerrain (흙 모양의 원본)
```

## 역할 분담

| 담당 | 하는 일 |
|---|---|
| UE (`AExcavatorPawn`, `ADigTerrain`) | 입력 읽기, 관절 각도대로 모델 배치, 지형 깎기·쌓기, 충돌, 렌더링, VR |
| 물리 백엔드 (`IExcavatorPhysics` 구현) | 관절 각도·속도, 주행 속도, 버킷이 받는 굴착 저항력 계산 |

**지형 원본은 UE가 가집니다.** 물리 백엔드는 흙 모양을 바꾸지 않고, 버킷이 받는 힘만 계산합니다.

## 주고받는 값 (매 프레임)

**UE → 물리**

| 구조체 | 필드 | 의미 |
|---|---|---|
| `FExcavatorCommand` | `Swing`, `Boom`, `Arm`, `Bucket`, `LeftTrack`, `RightTrack` | 스틱 명령, -1 ~ 1 |
| `FBucketSoilProbe` | `TipLocation`, `TipVelocity` | 버킷 날 위치(cm)와 속도(cm/s), 월드 좌표 |
| | `CutDirection` | 버킷 핀에서 날 방향 단위벡터 |
| | `SoilSurfaceZ`, `CutDepth` | 날 아래 지형 높이, 흙에 파묻힌 깊이(cm) |
| | `BucketLoadCm3` | 버킷에 담긴 흙 부피(cm³). 백엔드가 적재 질량으로 사용 |
| | `bInSoil` | 날이 지형 안에 있는지 |

**물리 → UE** (`FExcavatorPhysicsState`)

| 필드 | 의미 |
|---|---|
| `Joints` | 선회·붐·암·버킷 각도(°)와 각속도(°/s), 부모 링크 기준 |
| `ForwardSpeed`, `YawRate` | 하부 주행체 전진 속도(cm/s), 회전 속도(°/s) |
| `BucketResistance` | 흙이 버킷 날에 주는 힘(N), 월드 좌표. 단순 모델은 0 |

단위는 UE 기본(cm, °)을 씁니다. Chrono(m, rad)와는 백엔드 안에서 변환합니다.

## 백엔드

| 백엔드 | 상태 | 설명 |
|---|---|---|
| `FKinematicExcavatorPhysics` | 사용 중 | 1단계 모델. 관절이 명령 속도로 부드럽게 가속하고 한계각에서 멈춤. 내부 240 Hz 고정 스텝 |
| 유압 모델 (HTML `Sim` 이식) | 보류 | 2펌프 IM 유압 모델을 C++로 옮기면 이 자리에 들어감 |
| Chrono 모델 | 2b단계 | 아래 참고 |

## 2b단계: Chrono를 UE에 넣을 때

1. Chrono를 UE와 같은 MSVC로 DLL 빌드하고 `Source/ThirdParty/Chrono`에 둡니다. `ExcaSimulator.Build.cs`에 include/lib 경로와 DLL 복사(RuntimeDependencies)를 추가합니다.
2. Chrono 헤더를 쓰는 모듈은 `bEnableExceptions = true`, `bUseRTTI = true`가 필요합니다. 영향을 줄이려면 Chrono 전용 모듈(`ExcaChrono`)을 따로 만들고 거기서만 켭니다. Eigen 경고나 매크로 충돌은 `THIRD_PARTY_INCLUDES_START` / `END`로 감쌉니다.
3. `FChronoExcavatorPhysics : IExcavatorPhysics`를 만듭니다.
   - `Advance()`는 바로 리턴하고, 전용 스레드가 1 ms 고정 스텝으로 Chrono를 돌립니다.
   - `SetCommand`, `SetSoilProbe`는 잠금 후 복사, `GetState`는 이중 버퍼에서 최신 결과를 읽습니다. VR 프레임이 물리 계산을 기다리지 않게 하기 위해서입니다.
4. 굴착 저항력은 Chrono 쪽에서 `FBucketSoilProbe`의 깊이, 속도, 방향과 흙 물성값으로 계산합니다(예: McKyes 계열 공식). "Auto Dig & Dump Control" 모델과 같은 식을 쓰면 결과를 서로 비교할 수 있습니다.
5. `AExcavatorPawn` 생성자에서 만드는 백엔드만 바꾸면 됩니다. 나머지 코드는 그대로입니다.

## 2a단계: UE 지형 (이미 있는 `ADigTerrain`을 키우는 방향)

- 지금: CPU 높이 격자(20 cm) + 프로시저럴 메시 청크, 버킷 부피만큼 깎기, 안식각 34° 무너짐, 흙덩이 물리 후 지형으로 합치기.
- 다음:
  - 작업 구역만 5 cm 격자로 촘촘하게, 바깥은 성기게
  - 바뀐 영역만 GPU 텍스처로 올려서 그리기 (메시 재생성 비용 줄이기)
  - 바뀐 청크의 충돌 형상만 갱신 (지금도 청크 단위로 다시 만들지만, 비용 측정 필요)
  - 쏟을 때 Niagara 흙 파티클 추가
