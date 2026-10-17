"""Release oracle for the 0x007C0000 AI-state methods still open after gamelogic7c.

Each case runs the shipped bytes in Unicorn and the C++ port in ra3_aistate7ccheck.
EAX, object bytes and the call trace must match. Pointers into the emulated stack
or the port's frame are logged and compared as s+offset from the entry ESP.
"""

from __future__ import annotations

import json
import struct
import subprocess
import sys
from pathlib import Path

import capstone
import unicorn
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from emulate import STACK_BASE, STACK_SIZE, Emulator  # noqa: E402


class SiteEmulator(Emulator):
    """Honor a per-call cleanup returned by the stub as ``(eax, cleanup)``."""

    def _return_from_stub(self, stub, stack_cleanup):
        esp = self.uc.reg_read(uc.UC_X86_REG_ESP)
        return_address = struct.unpack("<I", self.uc.mem_read(esp, 4))[0]
        result = stub(self)
        if isinstance(result, tuple):
            eax, stack_cleanup = result
        else:
            eax = result
        self.uc.reg_write(uc.UC_X86_REG_EAX, int(eax) & 0xFFFFFFFF)
        self.uc.reg_write(uc.UC_X86_REG_ESP, (esp + 4 + int(stack_cleanup)) & 0xFFFFFFFF)
        self.uc.reg_write(uc.UC_X86_REG_EIP, return_address)
from unicorn import x86_const as uc  # noqa: E402

ROOT = Path(__file__).resolve().parents[4]
META_JSON = r'''[
  {
    "va": 8143296,
    "size": 93,
    "name": "AIAimFireState_onEnter",
    "cls": "AIAimFireState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12765456,
        "slot": 4,
        "name": "AIAimFireState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8143323,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 592,
        "tail": false
      },
      {
        "site": 8143343,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 76,
        "tail": false
      },
      {
        "site": 8143359,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8013968,
        "disp": null,
        "tail": false
      },
      {
        "site": 8143375,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8142912,
        "disp": null,
        "tail": false
      }
    ]
  },
  {
    "va": 8146656,
    "size": 120,
    "name": "AIBurningDeathState_update",
    "cls": "AIBurningDeathState",
    "slot": 5,
    "vtables": [
      {
        "vt": 12779672,
        "slot": 5,
        "name": "AIBurningDeathState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8146704,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 7638384,
        "disp": null,
        "tail": false
      },
      {
        "site": 8146719,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8065744,
        "disp": null,
        "tail": false
      },
      {
        "site": 8146735,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 8021792,
        "disp": null,
        "tail": false
      },
      {
        "site": 8146764,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8128672,
        "disp": null,
        "tail": false
      }
    ]
  },
  {
    "va": 8164208,
    "size": 134,
    "name": "AIMoveToAndCollectState_onEnter",
    "cls": "AIMoveToAndCollectState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12784776,
        "slot": 4,
        "name": "AIMoveToAndCollectState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8164217,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6004528,
        "disp": null,
        "tail": false
      },
      {
        "site": 8164256,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 7636368,
        "disp": null,
        "tail": false
      },
      {
        "site": 8164287,
        "cleanup": 4,
        "argc": 1,
        "kind": "virtual",
        "target": null,
        "disp": 96,
        "tail": false
      },
      {
        "site": 8164298,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8129232,
        "disp": null,
        "tail": false
      },
      {
        "site": 8164314,
        "cleanup": 0,
        "argc": 1,
        "kind": "direct",
        "target": 4286128,
        "disp": null,
        "tail": false
      }
    ]
  },
  {
    "va": 8171632,
    "size": 147,
    "name": "AIHarvestApproachSiteState_onEnter",
    "cls": "AIHarvestApproachSiteState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12787256,
        "slot": 4,
        "name": "AIHarvestApproachSiteState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8171657,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 452,
        "tail": false
      },
      {
        "site": 8171676,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6004528,
        "disp": null,
        "tail": false
      },
      {
        "site": 8171702,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 56,
        "tail": false
      },
      {
        "site": 8171721,
        "cleanup": 4,
        "argc": 1,
        "kind": "virtual",
        "target": null,
        "disp": 84,
        "tail": false
      },
      {
        "site": 8171743,
        "cleanup": 4,
        "argc": 1,
        "kind": "virtual",
        "target": null,
        "disp": 120,
        "tail": false
      },
      {
        "site": 8171774,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8128672,
        "disp": null,
        "tail": true
      }
    ]
  },
  {
    "va": 8157568,
    "size": 151,
    "name": "UnitAIReturnForLandingState_onEnter",
    "cls": "UnitAIReturnForLandingState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12770896,
        "slot": 4,
        "name": "UnitAIReturnForLandingState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8157572,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8157344,
        "disp": null,
        "tail": false
      },
      {
        "site": 8157597,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 76,
        "tail": false
      },
      {
        "site": 8157619,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5948656,
        "disp": null,
        "tail": false
      },
      {
        "site": 8157633,
        "cleanup": 4,
        "argc": 1,
        "kind": "virtual",
        "target": null,
        "disp": 56,
        "tail": false
      },
      {
        "site": 8157645,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 5939840,
        "disp": null,
        "tail": false
      },
      {
        "site": 8157664,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5948656,
        "disp": null,
        "tail": false
      },
      {
        "site": 8157678,
        "cleanup": 4,
        "argc": 1,
        "kind": "virtual",
        "target": null,
        "disp": 56,
        "tail": false
      },
      {
        "site": 8157704,
        "cleanup": 4,
        "argc": 1,
        "kind": "virtual",
        "target": null,
        "disp": 28,
        "tail": false
      },
      {
        "site": 8157717,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 16,
        "tail": true
      }
    ]
  },
  {
    "va": 8167072,
    "size": 153,
    "name": "AIInternalMoveToState_onEnter",
    "cls": "AIInternalMoveToState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12786448,
        "slot": 4,
        "name": "AIInternalMoveToState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8167078,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6004528,
        "disp": null,
        "tail": false
      },
      {
        "site": 8167115,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6846480,
        "disp": null,
        "tail": false
      },
      {
        "site": 8167133,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 56,
        "tail": false
      },
      {
        "site": 8167153,
        "cleanup": 4,
        "argc": 1,
        "kind": "virtual",
        "target": null,
        "disp": 52,
        "tail": false
      },
      {
        "site": 8167187,
        "cleanup": 12,
        "argc": 3,
        "kind": "virtual",
        "target": null,
        "disp": 8,
        "tail": false
      },
      {
        "site": 8167211,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 7234400,
        "disp": null,
        "tail": false
      },
      {
        "site": 8167220,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8128672,
        "disp": null,
        "tail": true
      }
    ]
  },
  {
    "va": 8167808,
    "size": 159,
    "name": "AIInternalMoveToState_onEnter_007ca180",
    "cls": "AIInternalMoveToState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12786688,
        "slot": 4,
        "name": "AIInternalMoveToState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8167815,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6004528,
        "disp": null,
        "tail": false
      },
      {
        "site": 8167828,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6843568,
        "disp": null,
        "tail": false
      },
      {
        "site": 8167868,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6846480,
        "disp": null,
        "tail": false
      },
      {
        "site": 8167906,
        "cleanup": 8,
        "argc": 2,
        "kind": "virtual",
        "target": null,
        "disp": 28,
        "tail": false
      },
      {
        "site": 8167931,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 72,
        "tail": false
      },
      {
        "site": 8167940,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6004528,
        "disp": null,
        "tail": false
      },
      {
        "site": 8167948,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 7234400,
        "disp": null,
        "tail": false
      },
      {
        "site": 8167962,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8128672,
        "disp": null,
        "tail": true
      }
    ]
  },
  {
    "va": 8180656,
    "size": 161,
    "name": "AIMoveAndDeleteState_update",
    "cls": "AIMoveAndDeleteState",
    "slot": 5,
    "vtables": [
      {
        "vt": 12778568,
        "slot": 5,
        "name": "AIMoveAndDeleteState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8180761,
        "cleanup": 12,
        "argc": 3,
        "kind": "virtual",
        "target": null,
        "disp": 24,
        "tail": false
      },
      {
        "site": 8180776,
        "cleanup": 12,
        "argc": 3,
        "kind": "direct",
        "target": 6560848,
        "disp": null,
        "tail": false
      },
      {
        "site": 8180789,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8065744,
        "disp": null,
        "tail": false
      },
      {
        "site": 8180807,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 5168496,
        "disp": null,
        "tail": false
      }
    ]
  },
  {
    "va": 8165424,
    "size": 162,
    "name": "UnitAIStartMoveState_onEnter",
    "cls": "UnitAIStartMoveState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12761272,
        "slot": 4,
        "name": "UnitAIStartMoveState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8165449,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 6005008,
        "disp": null,
        "tail": false
      },
      {
        "site": 8165470,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8165168,
        "disp": null,
        "tail": false
      },
      {
        "site": 8165538,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 7151648,
        "disp": null,
        "tail": false
      }
    ]
  },
  {
    "va": 8166896,
    "size": 162,
    "name": "AIDockApproachState_onEnter",
    "cls": "AIDockApproachState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12786344,
        "slot": 4,
        "name": "AIDockApproachState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8166902,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6004528,
        "disp": null,
        "tail": false
      },
      {
        "site": 8166939,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6846480,
        "disp": null,
        "tail": false
      },
      {
        "site": 8166957,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 56,
        "tail": false
      },
      {
        "site": 8166977,
        "cleanup": 4,
        "argc": 1,
        "kind": "virtual",
        "target": null,
        "disp": 52,
        "tail": false
      },
      {
        "site": 8167011,
        "cleanup": 12,
        "argc": 3,
        "kind": "virtual",
        "target": null,
        "disp": 4,
        "tail": false
      },
      {
        "site": 8167039,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 72,
        "tail": false
      },
      {
        "site": 8167053,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8128672,
        "disp": null,
        "tail": true
      }
    ]
  },
  {
    "va": 8167232,
    "size": 174,
    "name": "AIInternalMoveToState_onEnter_007c9f40",
    "cls": "AIInternalMoveToState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12786528,
        "slot": 4,
        "name": "AIInternalMoveToState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8167239,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6004528,
        "disp": null,
        "tail": false
      },
      {
        "site": 8167268,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6846480,
        "disp": null,
        "tail": false
      },
      {
        "site": 8167286,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 56,
        "tail": false
      },
      {
        "site": 8167306,
        "cleanup": 4,
        "argc": 1,
        "kind": "virtual",
        "target": null,
        "disp": 52,
        "tail": false
      },
      {
        "site": 8167332,
        "cleanup": 8,
        "argc": 2,
        "kind": "virtual",
        "target": null,
        "disp": 20,
        "tail": false
      },
      {
        "site": 8167370,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 72,
        "tail": false
      },
      {
        "site": 8167379,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6004528,
        "disp": null,
        "tail": false
      },
      {
        "site": 8167387,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 7234400,
        "disp": null,
        "tail": false
      },
      {
        "site": 8167401,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8128672,
        "disp": null,
        "tail": true
      }
    ]
  },
  {
    "va": 8189552,
    "size": 177,
    "name": "AITNGuardAttackAggressorState_update",
    "cls": "AITNGuardAttackAggressorState",
    "slot": 5,
    "vtables": [
      {
        "vt": 12768784,
        "slot": 5,
        "name": "AITNGuardAttackAggressorState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8189622,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10623968,
        "disp": null,
        "tail": false
      },
      {
        "site": 8189645,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5289456,
        "disp": null,
        "tail": false
      },
      {
        "site": 8189675,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10624144,
        "disp": null,
        "tail": false
      },
      {
        "site": 8189686,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6844320,
        "disp": null,
        "tail": false
      },
      {
        "site": 8189708,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 8343584,
        "disp": null,
        "tail": false
      },
      {
        "site": 8189727,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 20,
        "tail": true
      }
    ]
  },
  {
    "va": 8161808,
    "size": 187,
    "name": "UnitAICombinedUnitState_update",
    "cls": "UnitAICombinedUnitState",
    "slot": 5,
    "vtables": [
      {
        "vt": 12772616,
        "slot": 5,
        "name": "UnitAICombinedUnitState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8161864,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10623968,
        "disp": null,
        "tail": false
      },
      {
        "site": 8161887,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5289456,
        "disp": null,
        "tail": false
      },
      {
        "site": 8161917,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10624144,
        "disp": null,
        "tail": false
      },
      {
        "site": 8161932,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6844320,
        "disp": null,
        "tail": false
      },
      {
        "site": 8161941,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6844320,
        "disp": null,
        "tail": false
      },
      {
        "site": 8161964,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 16,
        "tail": true
      },
      {
        "site": 8161978,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 7340256,
        "disp": null,
        "tail": false
      }
    ]
  },
  {
    "va": 8143392,
    "size": 188,
    "name": "AIAimFireState_update",
    "cls": "AIAimFireState",
    "slot": 5,
    "vtables": [
      {
        "vt": 12765456,
        "slot": 5,
        "name": "AIAimFireState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8143398,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8013968,
        "disp": null,
        "tail": false
      },
      {
        "site": 8143417,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8142912,
        "disp": null,
        "tail": false
      },
      {
        "site": 8143463,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 8014112,
        "disp": null,
        "tail": false
      },
      {
        "site": 8143482,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 7151648,
        "disp": null,
        "tail": false
      },
      {
        "site": 8143507,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 8014416,
        "disp": null,
        "tail": false
      },
      {
        "site": 8143529,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 7151648,
        "disp": null,
        "tail": false
      },
      {
        "site": 8143571,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 216,
        "tail": false
      }
    ]
  },
  {
    "va": 8164016,
    "size": 190,
    "name": "UnitAIIsBlockedByWall_onEnter",
    "cls": "UnitAIIsBlockedByWall",
    "slot": 4,
    "vtables": [
      {
        "vt": 12760416,
        "slot": 4,
        "name": "UnitAIIsBlockedByWall"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8164029,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6004528,
        "disp": null,
        "tail": false
      },
      {
        "site": 8164072,
        "cleanup": 0,
        "argc": 6,
        "kind": "direct",
        "target": 6597760,
        "disp": null,
        "tail": false
      },
      {
        "site": 8164115,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10623968,
        "disp": null,
        "tail": false
      },
      {
        "site": 8164138,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5289456,
        "disp": null,
        "tail": false
      },
      {
        "site": 8164168,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10624144,
        "disp": null,
        "tail": false
      },
      {
        "site": 8164176,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 7993664,
        "disp": null,
        "tail": false
      }
    ]
  },
  {
    "va": 8136208,
    "size": 195,
    "name": "AIAttackFireDuringApproachState_onEnter",
    "cls": "AIAttackFireDuringApproachState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12777704,
        "slot": 4,
        "name": "AIAttackFireDuringApproachState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8136218,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6020256,
        "disp": null,
        "tail": false
      },
      {
        "site": 8136250,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6004528,
        "disp": null,
        "tail": false
      },
      {
        "site": 8136284,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6938992,
        "disp": null,
        "tail": false
      },
      {
        "site": 8136297,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 6967712,
        "disp": null,
        "tail": false
      },
      {
        "site": 8136318,
        "cleanup": 16,
        "argc": 4,
        "kind": "direct",
        "target": 7283008,
        "disp": null,
        "tail": false
      },
      {
        "site": 8136354,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6919872,
        "disp": null,
        "tail": false
      },
      {
        "site": 8136366,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 76,
        "tail": false
      },
      {
        "site": 8136381,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8128672,
        "disp": null,
        "tail": false
      }
    ]
  },
  {
    "va": 8168544,
    "size": 195,
    "name": "AIGuardMoveToAttack_onEnter",
    "cls": "AIGuardMoveToAttack",
    "slot": 4,
    "vtables": [
      {
        "vt": 12786848,
        "slot": 4,
        "name": "AIGuardMoveToAttack"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8168557,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6971920,
        "disp": null,
        "tail": false
      },
      {
        "site": 8168592,
        "cleanup": 4,
        "argc": 1,
        "kind": "virtual",
        "target": null,
        "disp": 16,
        "tail": false
      },
      {
        "site": 8168671,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 628,
        "tail": false
      },
      {
        "site": 8168686,
        "cleanup": 0,
        "argc": 3,
        "kind": "direct",
        "target": 6534416,
        "disp": null,
        "tail": false
      },
      {
        "site": 8168696,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6919872,
        "disp": null,
        "tail": false
      },
      {
        "site": 8168712,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5948656,
        "disp": null,
        "tail": false
      },
      {
        "site": 8168727,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8128672,
        "disp": null,
        "tail": true
      }
    ]
  },
  {
    "va": 8172368,
    "size": 203,
    "name": "AIHarvestWorkSiteState_onExit",
    "cls": "AIHarvestWorkSiteState",
    "slot": 6,
    "vtables": [
      {
        "vt": 12762824,
        "slot": 6,
        "name": "AIHarvestWorkSiteState"
      }
    ],
    "ret": 4,
    "sites": [
      {
        "site": 8172417,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 6967168,
        "disp": null,
        "tail": false
      },
      {
        "site": 8172424,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6967216,
        "disp": null,
        "tail": false
      },
      {
        "site": 8172441,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 7151648,
        "disp": null,
        "tail": false
      },
      {
        "site": 8172475,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10623968,
        "disp": null,
        "tail": false
      },
      {
        "site": 8172498,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5289456,
        "disp": null,
        "tail": false
      },
      {
        "site": 8172528,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10624144,
        "disp": null,
        "tail": false
      },
      {
        "site": 8172538,
        "cleanup": 0,
        "argc": 1,
        "kind": "direct",
        "target": 6811952,
        "disp": null,
        "tail": false
      },
      {
        "site": 8172553,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 7178784,
        "disp": null,
        "tail": false
      }
    ]
  },
  {
    "va": 8157136,
    "size": 203,
    "name": "UnitAIMoveToRallyPointState_onEnter",
    "cls": "UnitAIMoveToRallyPointState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12770584,
        "slot": 4,
        "name": "UnitAIMoveToRallyPointState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8157186,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10623968,
        "disp": null,
        "tail": false
      },
      {
        "site": 8157209,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5289456,
        "disp": null,
        "tail": false
      },
      {
        "site": 8157239,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10624144,
        "disp": null,
        "tail": false
      },
      {
        "site": 8157252,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 7081552,
        "disp": null,
        "tail": false
      },
      {
        "site": 8157268,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 36,
        "tail": false
      },
      {
        "site": 8157283,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 76,
        "tail": false
      },
      {
        "site": 8157301,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5948656,
        "disp": null,
        "tail": false
      },
      {
        "site": 8157318,
        "cleanup": 4,
        "argc": 1,
        "kind": "virtual",
        "target": null,
        "disp": 28,
        "tail": false
      }
    ]
  },
  {
    "va": 8140192,
    "size": 209,
    "name": "AIAttackMoveToState_onEnter",
    "cls": "AIAttackMoveToState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12788096,
        "slot": 4,
        "name": "AIAttackMoveToState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8140217,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 52,
        "tail": false
      },
      {
        "site": 8140229,
        "cleanup": 4,
        "argc": 1,
        "kind": "virtual",
        "target": null,
        "disp": 28,
        "tail": false
      },
      {
        "site": 8140241,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 648,
        "tail": false
      },
      {
        "site": 8140293,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 6967168,
        "disp": null,
        "tail": false
      },
      {
        "site": 8140300,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6967216,
        "disp": null,
        "tail": false
      },
      {
        "site": 8140307,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8129232,
        "disp": null,
        "tail": false
      },
      {
        "site": 8140376,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 7151648,
        "disp": null,
        "tail": false
      },
      {
        "site": 8140390,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 7151648,
        "disp": null,
        "tail": false
      }
    ]
  },
  {
    "va": 8160336,
    "size": 212,
    "name": "SA_AIApproachTargetState_onEnter",
    "cls": "SA_AIApproachTargetState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12783176,
        "slot": 4,
        "name": "SA_AIApproachTargetState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8160363,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 8268112,
        "disp": null,
        "tail": false
      },
      {
        "site": 8160376,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 6846640,
        "disp": null,
        "tail": false
      },
      {
        "site": 8160391,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 7442336,
        "disp": null,
        "tail": false
      },
      {
        "site": 8160456,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 8159936,
        "disp": null,
        "tail": false
      },
      {
        "site": 8160490,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5948656,
        "disp": null,
        "tail": false
      },
      {
        "site": 8160511,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 76,
        "tail": false
      },
      {
        "site": 8160522,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 5973072,
        "disp": null,
        "tail": false
      },
      {
        "site": 8160539,
        "cleanup": 4,
        "argc": 1,
        "kind": "virtual",
        "target": null,
        "disp": 28,
        "tail": false
      }
    ]
  },
  {
    "va": 8137184,
    "size": 214,
    "name": "AIAttackMeleeSquishState_onEnter",
    "cls": "AIAttackMeleeSquishState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12777928,
        "slot": 4,
        "name": "AIAttackMeleeSquishState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8137240,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6020256,
        "disp": null,
        "tail": false
      },
      {
        "site": 8137272,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6004528,
        "disp": null,
        "tail": false
      },
      {
        "site": 8137293,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6938992,
        "disp": null,
        "tail": false
      },
      {
        "site": 8137307,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 7367216,
        "disp": null,
        "tail": false
      },
      {
        "site": 8137343,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6919872,
        "disp": null,
        "tail": false
      },
      {
        "site": 8137355,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 76,
        "tail": false
      },
      {
        "site": 8137374,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 7151648,
        "disp": null,
        "tail": false
      },
      {
        "site": 8137384,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8128672,
        "disp": null,
        "tail": true
      }
    ]
  },
  {
    "va": 8167408,
    "size": 215,
    "name": "AIInternalMoveToState_onEnter_007c9ff0",
    "cls": "AIInternalMoveToState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12786608,
        "slot": 4,
        "name": "AIInternalMoveToState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8167415,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6004528,
        "disp": null,
        "tail": false
      },
      {
        "site": 8167444,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6846480,
        "disp": null,
        "tail": false
      },
      {
        "site": 8167462,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 56,
        "tail": false
      },
      {
        "site": 8167482,
        "cleanup": 4,
        "argc": 1,
        "kind": "virtual",
        "target": null,
        "disp": 52,
        "tail": false
      },
      {
        "site": 8167508,
        "cleanup": 8,
        "argc": 2,
        "kind": "virtual",
        "target": null,
        "disp": 24,
        "tail": false
      },
      {
        "site": 8167533,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 72,
        "tail": false
      },
      {
        "site": 8167542,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6004528,
        "disp": null,
        "tail": false
      },
      {
        "site": 8167550,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 7234400,
        "disp": null,
        "tail": false
      },
      {
        "site": 8167567,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 5939776,
        "disp": null,
        "tail": false
      },
      {
        "site": 8167580,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 7783424,
        "disp": null,
        "tail": false
      },
      {
        "site": 8167592,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 80,
        "tail": false
      },
      {
        "site": 8167609,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 7151648,
        "disp": null,
        "tail": false
      },
      {
        "site": 8167618,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8128672,
        "disp": null,
        "tail": true
      }
    ]
  },
  {
    "va": 8146432,
    "size": 220,
    "name": "AIBurningDeathState_onEnter",
    "cls": "AIBurningDeathState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12779672,
        "slot": 4,
        "name": "AIBurningDeathState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8146477,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6828368,
        "disp": null,
        "tail": false
      },
      {
        "site": 8146518,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 7277088,
        "disp": null,
        "tail": false
      },
      {
        "site": 8146530,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 5085936,
        "disp": null,
        "tail": false
      },
      {
        "site": 8146544,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 7151760,
        "disp": null,
        "tail": false
      },
      {
        "site": 8146561,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5065632,
        "disp": null,
        "tail": false
      },
      {
        "site": 8146574,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 7677888,
        "disp": null,
        "tail": false
      },
      {
        "site": 8146590,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 5390528,
        "disp": null,
        "tail": false
      },
      {
        "site": 8146602,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 8021792,
        "disp": null,
        "tail": false
      },
      {
        "site": 8146636,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8128672,
        "disp": null,
        "tail": true
      }
    ]
  },
  {
    "va": 8187392,
    "size": 226,
    "name": "AITNGuardInnerState_onEnter",
    "cls": "AITNGuardInnerState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12768488,
        "slot": 4,
        "name": "AITNGuardInnerState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8187435,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10623968,
        "disp": null,
        "tail": false
      },
      {
        "site": 8187458,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5289456,
        "disp": null,
        "tail": false
      },
      {
        "site": 8187488,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10624144,
        "disp": null,
        "tail": false
      },
      {
        "site": 8187544,
        "cleanup": 0,
        "argc": 1,
        "kind": "direct",
        "target": 4286496,
        "disp": null,
        "tail": false
      },
      {
        "site": 8187572,
        "cleanup": 20,
        "argc": 5,
        "kind": "direct",
        "target": 8017552,
        "disp": null,
        "tail": false
      },
      {
        "site": 8187595,
        "cleanup": 4,
        "argc": 1,
        "kind": "virtual",
        "target": null,
        "disp": 56,
        "tail": false
      },
      {
        "site": 8187605,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 16,
        "tail": false
      }
    ]
  },
  {
    "va": 8188304,
    "size": 227,
    "name": "AITNGuardOuterState_update",
    "cls": "AITNGuardOuterState",
    "slot": 5,
    "vtables": [
      {
        "vt": 12768584,
        "slot": 5,
        "name": "AITNGuardOuterState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8188329,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6004528,
        "disp": null,
        "tail": false
      },
      {
        "site": 8188379,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10623968,
        "disp": null,
        "tail": false
      },
      {
        "site": 8188402,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5289456,
        "disp": null,
        "tail": false
      },
      {
        "site": 8188432,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10624144,
        "disp": null,
        "tail": false
      },
      {
        "site": 8188447,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6847632,
        "disp": null,
        "tail": false
      },
      {
        "site": 8188466,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6847632,
        "disp": null,
        "tail": false
      },
      {
        "site": 8188473,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8324880,
        "disp": null,
        "tail": false
      },
      {
        "site": 8188495,
        "cleanup": 4,
        "argc": 1,
        "kind": "virtual",
        "target": null,
        "disp": 56,
        "tail": false
      },
      {
        "site": 8188512,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 16,
        "tail": true
      },
      {
        "site": 8188529,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 20,
        "tail": true
      }
    ]
  },
  {
    "va": 8145968,
    "size": 243,
    "name": "AIMoveToPositionAndEnterState_onEnter",
    "cls": "AIMoveToPositionAndEnterState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12779960,
        "slot": 4,
        "name": "AIMoveToPositionAndEnterState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8146004,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6004528,
        "disp": null,
        "tail": false
      },
      {
        "site": 8146016,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 6977056,
        "disp": null,
        "tail": false
      },
      {
        "site": 8146031,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 328,
        "tail": false
      },
      {
        "site": 8146058,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 7234400,
        "disp": null,
        "tail": false
      },
      {
        "site": 8146092,
        "cleanup": 0,
        "argc": 3,
        "kind": "direct",
        "target": 6534416,
        "disp": null,
        "tail": false
      },
      {
        "site": 8146102,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8128672,
        "disp": null,
        "tail": false
      },
      {
        "site": 8146124,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6922288,
        "disp": null,
        "tail": false
      },
      {
        "site": 8146143,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8051376,
        "disp": null,
        "tail": false
      },
      {
        "site": 8146159,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 6827232,
        "disp": null,
        "tail": false
      },
      {
        "site": 8146169,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 6977056,
        "disp": null,
        "tail": false
      },
      {
        "site": 8146188,
        "cleanup": 8,
        "argc": 2,
        "kind": "virtual",
        "target": null,
        "disp": 64,
        "tail": false
      }
    ]
  },
  {
    "va": 8164912,
    "size": 243,
    "name": "UnitAIReturnToSourceAndEnterState_onEnter",
    "cls": "UnitAIReturnToSourceAndEnterState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12775888,
        "slot": 4,
        "name": "UnitAIReturnToSourceAndEnterState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8164925,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 76,
        "tail": false
      },
      {
        "site": 8164972,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10623968,
        "disp": null,
        "tail": false
      },
      {
        "site": 8164995,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5289456,
        "disp": null,
        "tail": false
      },
      {
        "site": 8165025,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10624144,
        "disp": null,
        "tail": false
      },
      {
        "site": 8165051,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5948656,
        "disp": null,
        "tail": false
      },
      {
        "site": 8165088,
        "cleanup": 4,
        "argc": 1,
        "kind": "virtual",
        "target": null,
        "disp": 56,
        "tail": false
      },
      {
        "site": 8165101,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 6977056,
        "disp": null,
        "tail": false
      },
      {
        "site": 8165120,
        "cleanup": 8,
        "argc": 2,
        "kind": "virtual",
        "target": null,
        "disp": 64,
        "tail": false
      },
      {
        "site": 8165134,
        "cleanup": 4,
        "argc": 1,
        "kind": "virtual",
        "target": null,
        "disp": 28,
        "tail": false
      }
    ]
  },
  {
    "va": 8188048,
    "size": 243,
    "name": "AITNGuardOuterState_onEnter",
    "cls": "AITNGuardOuterState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12768584,
        "slot": 4,
        "name": "AITNGuardOuterState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8188108,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10623968,
        "disp": null,
        "tail": false
      },
      {
        "site": 8188131,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5289456,
        "disp": null,
        "tail": false
      },
      {
        "site": 8188161,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10624144,
        "disp": null,
        "tail": false
      },
      {
        "site": 8188217,
        "cleanup": 0,
        "argc": 1,
        "kind": "direct",
        "target": 4286496,
        "disp": null,
        "tail": false
      },
      {
        "site": 8188245,
        "cleanup": 20,
        "argc": 5,
        "kind": "direct",
        "target": 8017552,
        "disp": null,
        "tail": false
      },
      {
        "site": 8188268,
        "cleanup": 4,
        "argc": 1,
        "kind": "virtual",
        "target": null,
        "disp": 56,
        "tail": false
      },
      {
        "site": 8188278,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 16,
        "tail": false
      }
    ]
  },
  {
    "va": 8169408,
    "size": 252,
    "name": "AIGuardReturnState_update",
    "cls": "AIGuardReturnState",
    "slot": 5,
    "vtables": [
      {
        "vt": 12787056,
        "slot": 5,
        "name": "AIGuardReturnState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8169447,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 556,
        "tail": false
      },
      {
        "site": 8169475,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10623968,
        "disp": null,
        "tail": false
      },
      {
        "site": 8169498,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5289456,
        "disp": null,
        "tail": false
      },
      {
        "site": 8169528,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10624144,
        "disp": null,
        "tail": false
      },
      {
        "site": 8169543,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 560,
        "tail": false
      },
      {
        "site": 8169552,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 8306144,
        "disp": null,
        "tail": false
      },
      {
        "site": 8169591,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 8362544,
        "disp": null,
        "tail": false
      },
      {
        "site": 8169630,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5948656,
        "disp": null,
        "tail": false
      },
      {
        "site": 8169644,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8065744,
        "disp": null,
        "tail": true
      }
    ]
  },
  {
    "va": 8164352,
    "size": 275,
    "name": "UnitAIMoveToGoalState_update",
    "cls": "UnitAIMoveToGoalState",
    "slot": 5,
    "vtables": [
      {
        "vt": 12785256,
        "slot": 5,
        "name": "UnitAIMoveToGoalState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8164362,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6004528,
        "disp": null,
        "tail": false
      },
      {
        "site": 8164392,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 16,
        "tail": false
      },
      {
        "site": 8164408,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 7151648,
        "disp": null,
        "tail": false
      },
      {
        "site": 8164417,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 6967744,
        "disp": null,
        "tail": false
      },
      {
        "site": 8164435,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6840032,
        "disp": null,
        "tail": false
      },
      {
        "site": 8164462,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 592,
        "tail": false
      },
      {
        "site": 8164513,
        "cleanup": 0,
        "argc": 4,
        "kind": "direct",
        "target": 7997264,
        "disp": null,
        "tail": false
      },
      {
        "site": 8164554,
        "cleanup": 20,
        "argc": 5,
        "kind": "direct",
        "target": 6953264,
        "disp": null,
        "tail": false
      },
      {
        "site": 8164575,
        "cleanup": 0,
        "argc": 6,
        "kind": "direct",
        "target": 8049360,
        "disp": null,
        "tail": false
      },
      {
        "site": 8164594,
        "cleanup": 0,
        "argc": 3,
        "kind": "direct",
        "target": 8087936,
        "disp": null,
        "tail": false
      },
      {
        "site": 8164612,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 7151648,
        "disp": null,
        "tail": false
      }
    ]
  },
  {
    "va": 8160704,
    "size": 277,
    "name": "UnitAIAttackBlockingObjectState_onEnter",
    "cls": "UnitAIAttackBlockingObjectState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12783328,
        "slot": 4,
        "name": "UnitAIAttackBlockingObjectState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8160722,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6004528,
        "disp": null,
        "tail": false
      },
      {
        "site": 8160745,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 76,
        "tail": false
      },
      {
        "site": 8160774,
        "cleanup": 0,
        "argc": 6,
        "kind": "direct",
        "target": 6597760,
        "disp": null,
        "tail": false
      },
      {
        "site": 8160821,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10623968,
        "disp": null,
        "tail": false
      },
      {
        "site": 8160848,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5289456,
        "disp": null,
        "tail": false
      },
      {
        "site": 8160878,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10624144,
        "disp": null,
        "tail": false
      },
      {
        "site": 8160903,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 7081792,
        "disp": null,
        "tail": false
      },
      {
        "site": 8160928,
        "cleanup": 4,
        "argc": 1,
        "kind": "virtual",
        "target": null,
        "disp": 56,
        "tail": false
      },
      {
        "site": 8160947,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5948656,
        "disp": null,
        "tail": false
      },
      {
        "site": 8160955,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 7152112,
        "disp": null,
        "tail": false
      },
      {
        "site": 8160972,
        "cleanup": 4,
        "argc": 1,
        "kind": "virtual",
        "target": null,
        "disp": 28,
        "tail": false
      }
    ]
  },
  {
    "va": 8144640,
    "size": 282,
    "name": "AIEnterAndAttackState_onEnter",
    "cls": "AIEnterAndAttackState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12779200,
        "slot": 4,
        "name": "AIEnterAndAttackState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8144660,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6004528,
        "disp": null,
        "tail": false
      },
      {
        "site": 8144679,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6971920,
        "disp": null,
        "tail": false
      },
      {
        "site": 8144724,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 648,
        "tail": false
      },
      {
        "site": 8144753,
        "cleanup": 20,
        "argc": 5,
        "kind": "direct",
        "target": 8272128,
        "disp": null,
        "tail": false
      },
      {
        "site": 8144771,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 6977056,
        "disp": null,
        "tail": false
      },
      {
        "site": 8144793,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 324,
        "tail": false
      },
      {
        "site": 8144823,
        "cleanup": 8,
        "argc": 2,
        "kind": "virtual",
        "target": null,
        "disp": 64,
        "tail": false
      },
      {
        "site": 8144860,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6004528,
        "disp": null,
        "tail": false
      },
      {
        "site": 8144872,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 7234400,
        "disp": null,
        "tail": false
      },
      {
        "site": 8144904,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8128672,
        "disp": null,
        "tail": true
      }
    ]
  },
  {
    "va": 8184640,
    "size": 287,
    "name": "AIEnterAndAttackState_onExit",
    "cls": "AIEnterAndAttackState",
    "slot": 6,
    "vtables": [
      {
        "vt": 12779200,
        "slot": 6,
        "name": "AIEnterAndAttackState"
      }
    ],
    "ret": 4,
    "sites": [
      {
        "site": 8184677,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10623968,
        "disp": null,
        "tail": false
      },
      {
        "site": 8184700,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5289456,
        "disp": null,
        "tail": false
      },
      {
        "site": 8184730,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10624144,
        "disp": null,
        "tail": false
      },
      {
        "site": 8184748,
        "cleanup": 4,
        "argc": 1,
        "kind": "virtual",
        "target": null,
        "disp": 56,
        "tail": false
      },
      {
        "site": 8184763,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 8065424,
        "disp": null,
        "tail": false
      },
      {
        "site": 8184782,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 7234400,
        "disp": null,
        "tail": false
      },
      {
        "site": 8184830,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10623968,
        "disp": null,
        "tail": false
      },
      {
        "site": 8184853,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5289456,
        "disp": null,
        "tail": false
      },
      {
        "site": 8184883,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10624144,
        "disp": null,
        "tail": false
      },
      {
        "site": 8184897,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 6977056,
        "disp": null,
        "tail": false
      },
      {
        "site": 8184916,
        "cleanup": 8,
        "argc": 2,
        "kind": "virtual",
        "target": null,
        "disp": 64,
        "tail": false
      }
    ]
  },
  {
    "va": 8185440,
    "size": 291,
    "name": "AIMoveToPositionAndEnterState_onExit",
    "cls": "AIMoveToPositionAndEnterState",
    "slot": 6,
    "vtables": [
      {
        "vt": 12779960,
        "slot": 6,
        "name": "AIMoveToPositionAndEnterState"
      }
    ],
    "ret": 4,
    "sites": [
      {
        "site": 8185479,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 7151648,
        "disp": null,
        "tail": false
      },
      {
        "site": 8185490,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 7151648,
        "disp": null,
        "tail": false
      },
      {
        "site": 8185517,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 7165296,
        "disp": null,
        "tail": false
      },
      {
        "site": 8185528,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 7151648,
        "disp": null,
        "tail": false
      },
      {
        "site": 8185545,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 7234400,
        "disp": null,
        "tail": false
      },
      {
        "site": 8185579,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10623968,
        "disp": null,
        "tail": false
      },
      {
        "site": 8185602,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5289456,
        "disp": null,
        "tail": false
      },
      {
        "site": 8185632,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10624144,
        "disp": null,
        "tail": false
      },
      {
        "site": 8185643,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6977024,
        "disp": null,
        "tail": false
      },
      {
        "site": 8185662,
        "cleanup": 8,
        "argc": 2,
        "kind": "virtual",
        "target": null,
        "disp": 64,
        "tail": false
      },
      {
        "site": 8185671,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 8065424,
        "disp": null,
        "tail": false
      },
      {
        "site": 8185716,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 7041088,
        "disp": null,
        "tail": false
      }
    ]
  },
  {
    "va": 8169088,
    "size": 305,
    "name": "AIGuardReturnState_onEnter",
    "cls": "AIGuardReturnState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12787056,
        "slot": 4,
        "name": "AIGuardReturnState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8169127,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 556,
        "tail": false
      },
      {
        "site": 8169155,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10623968,
        "disp": null,
        "tail": false
      },
      {
        "site": 8169178,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5289456,
        "disp": null,
        "tail": false
      },
      {
        "site": 8169208,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10624144,
        "disp": null,
        "tail": false
      },
      {
        "site": 8169223,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 560,
        "tail": false
      },
      {
        "site": 8169232,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 8306144,
        "disp": null,
        "tail": false
      },
      {
        "site": 8169274,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 8362544,
        "disp": null,
        "tail": false
      },
      {
        "site": 8169312,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 628,
        "tail": false
      },
      {
        "site": 8169333,
        "cleanup": 0,
        "argc": 3,
        "kind": "direct",
        "target": 6534416,
        "disp": null,
        "tail": false
      },
      {
        "site": 8169343,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6919872,
        "disp": null,
        "tail": false
      },
      {
        "site": 8169359,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5948656,
        "disp": null,
        "tail": false
      },
      {
        "site": 8169377,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8128672,
        "disp": null,
        "tail": true
      }
    ]
  },
  {
    "va": 8185120,
    "size": 310,
    "name": "AIMoveToPositionAndDieState_update",
    "cls": "AIMoveToPositionAndDieState",
    "slot": 5,
    "vtables": [
      {
        "vt": 12779448,
        "slot": 5,
        "name": "AIMoveToPositionAndDieState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8185231,
        "cleanup": 12,
        "argc": 3,
        "kind": "virtual",
        "target": null,
        "disp": 24,
        "tail": false
      },
      {
        "site": 8185246,
        "cleanup": 12,
        "argc": 3,
        "kind": "direct",
        "target": 6560848,
        "disp": null,
        "tail": false
      },
      {
        "site": 8185257,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8065744,
        "disp": null,
        "tail": false
      },
      {
        "site": 8185283,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 100,
        "tail": false
      },
      {
        "site": 8185300,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 100,
        "tail": false
      },
      {
        "site": 8185335,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10623968,
        "disp": null,
        "tail": false
      },
      {
        "site": 8185358,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5289456,
        "disp": null,
        "tail": false
      },
      {
        "site": 8185388,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10624144,
        "disp": null,
        "tail": false
      },
      {
        "site": 8185402,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 7368224,
        "disp": null,
        "tail": false
      },
      {
        "site": 8185415,
        "cleanup": 12,
        "argc": 3,
        "kind": "direct",
        "target": 7635872,
        "disp": null,
        "tail": false
      }
    ]
  },
  {
    "va": 8189232,
    "size": 313,
    "name": "AITNGuardAttackAggressorState_onEnter",
    "cls": "AITNGuardAttackAggressorState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12768784,
        "slot": 4,
        "name": "AITNGuardAttackAggressorState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8189261,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 100,
        "tail": false
      },
      {
        "site": 8189280,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 100,
        "tail": false
      },
      {
        "site": 8189329,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10623968,
        "disp": null,
        "tail": false
      },
      {
        "site": 8189352,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5289456,
        "disp": null,
        "tail": false
      },
      {
        "site": 8189382,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 10624144,
        "disp": null,
        "tail": false
      },
      {
        "site": 8189407,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6844320,
        "disp": null,
        "tail": false
      },
      {
        "site": 8189429,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 8343584,
        "disp": null,
        "tail": false
      },
      {
        "site": 8189471,
        "cleanup": 0,
        "argc": 1,
        "kind": "direct",
        "target": 4286496,
        "disp": null,
        "tail": false
      },
      {
        "site": 8189499,
        "cleanup": 20,
        "argc": 5,
        "kind": "direct",
        "target": 8017552,
        "disp": null,
        "tail": false
      },
      {
        "site": 8189522,
        "cleanup": 4,
        "argc": 1,
        "kind": "virtual",
        "target": null,
        "disp": 56,
        "tail": false
      },
      {
        "site": 8189532,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 16,
        "tail": false
      }
    ]
  },
  {
    "va": 8144320,
    "size": 314,
    "name": "AIEnterState_onEnter",
    "cls": "AIEnterState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12779008,
        "slot": 4,
        "name": "AIEnterState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8144361,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6004528,
        "disp": null,
        "tail": false
      },
      {
        "site": 8144390,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 648,
        "tail": false
      },
      {
        "site": 8144421,
        "cleanup": 20,
        "argc": 5,
        "kind": "direct",
        "target": 8272128,
        "disp": null,
        "tail": false
      },
      {
        "site": 8144440,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 6977056,
        "disp": null,
        "tail": false
      },
      {
        "site": 8144462,
        "cleanup": 0,
        "argc": 0,
        "kind": "virtual",
        "target": null,
        "disp": 324,
        "tail": false
      },
      {
        "site": 8144515,
        "cleanup": 0,
        "argc": 3,
        "kind": "direct",
        "target": 6534416,
        "disp": null,
        "tail": false
      },
      {
        "site": 8144534,
        "cleanup": 8,
        "argc": 2,
        "kind": "virtual",
        "target": null,
        "disp": 64,
        "tail": false
      },
      {
        "site": 8144577,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6004528,
        "disp": null,
        "tail": false
      },
      {
        "site": 8144585,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 7234400,
        "disp": null,
        "tail": false
      },
      {
        "site": 8144617,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8128672,
        "disp": null,
        "tail": true
      }
    ]
  },
  {
    "va": 8141280,
    "size": 330,
    "name": "AIFollowWaypointPathExactState_onEnter",
    "cls": "AIFollowWaypointPathExactState",
    "slot": 4,
    "vtables": [
      {
        "vt": 12778784,
        "slot": 4,
        "name": "AIFollowWaypointPathExactState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8141346,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 5948656,
        "disp": null,
        "tail": false
      },
      {
        "site": 8141398,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6922288,
        "disp": null,
        "tail": false
      },
      {
        "site": 8141419,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8051376,
        "disp": null,
        "tail": false
      },
      {
        "site": 8141444,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 8050672,
        "disp": null,
        "tail": false
      },
      {
        "site": 8141517,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8128672,
        "disp": null,
        "tail": false
      },
      {
        "site": 8141532,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 7039456,
        "disp": null,
        "tail": false
      },
      {
        "site": 8141595,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 6827232,
        "disp": null,
        "tail": false
      }
    ]
  },
  {
    "va": 8145552,
    "size": 333,
    "name": "AIChargeTargetState_update",
    "cls": "AIChargeTargetState",
    "slot": 5,
    "vtables": [
      {
        "vt": 12779344,
        "slot": 5,
        "name": "AIChargeTargetState"
      }
    ],
    "ret": 0,
    "sites": [
      {
        "site": 8145567,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6004528,
        "disp": null,
        "tail": false
      },
      {
        "site": 8145625,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8085792,
        "disp": null,
        "tail": false
      },
      {
        "site": 8145650,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6843904,
        "disp": null,
        "tail": false
      },
      {
        "site": 8145739,
        "cleanup": 8,
        "argc": 2,
        "kind": "direct",
        "target": 7677424,
        "disp": null,
        "tail": false
      },
      {
        "site": 8145748,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 6967712,
        "disp": null,
        "tail": false
      },
      {
        "site": 8145767,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8020608,
        "disp": null,
        "tail": false
      },
      {
        "site": 8145793,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 6850576,
        "disp": null,
        "tail": false
      },
      {
        "site": 8145807,
        "cleanup": 12,
        "argc": 3,
        "kind": "direct",
        "target": 8085872,
        "disp": null,
        "tail": false
      },
      {
        "site": 8145819,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 5085936,
        "disp": null,
        "tail": false
      },
      {
        "site": 8145828,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 7677888,
        "disp": null,
        "tail": false
      },
      {
        "site": 8145836,
        "cleanup": 4,
        "argc": 1,
        "kind": "direct",
        "target": 7975664,
        "disp": null,
        "tail": false
      },
      {
        "site": 8145867,
        "cleanup": 0,
        "argc": 0,
        "kind": "direct",
        "target": 8065744,
        "disp": null,
        "tail": true
      }
    ]
  }
]'''
META = json.loads(META_JSON)
SIZE = 0x800
GAME_LOGIC = 0x00DEBA64
EAX_IN = 0xA5A5A5A5
POOL = 16
VT_INDEX = POOL - 1
ACTIVE = {"box": None, "by_disp": {}}
SLOT_ADDR: dict[int, int] = {}
TRACE: list[tuple[int, dict[str, int]]] = []
REG = {
    "eax": uc.UC_X86_REG_EAX, "ebx": uc.UC_X86_REG_EBX, "ecx": uc.UC_X86_REG_ECX,
    "edx": uc.UC_X86_REG_EDX, "esi": uc.UC_X86_REG_ESI, "edi": uc.UC_X86_REG_EDI,
    "ebp": uc.UC_X86_REG_EBP, "esp": uc.UC_X86_REG_ESP,
}
XMM = [getattr(uc, f"UC_X86_REG_XMM{i}") for i in range(8)]


def dword(blob: bytes, offset: int) -> int:
    return struct.unpack_from("<I", blob, offset)[0]


def entry_esp(argc: int) -> int:
    top = STACK_BASE + STACK_SIZE - 0x100
    return top - (4 + 4 * argc)


def stack_token(value: int, entry: int) -> str:
    delta = (value - entry) & 0xFFFFFFFF
    if delta >= 0x80000000:
        return f"s-{0x100000000 - delta:x}"
    return f"s+{delta:x}"


def norm(value: int, bases: list[int], entry: int) -> str:
    for index, base in enumerate(bases):
        if base <= value < base + SIZE:
            return f"@{index}+{value - base:x}"
    if STACK_BASE <= value < STACK_BASE + STACK_SIZE:
        return stack_token(value, entry)
    return f"{value & 0xFFFFFFFF:08x}"


def canon(buf: bytes, bases: list[int], entry: int, stack_lo: int, stack_hi: int) -> bytes:
    out = bytearray(buf)
    for offset in range(0, len(out) - 3, 4):
        value = struct.unpack_from("<I", out, offset)[0]
        replaced = False
        for index, base in enumerate(bases):
            if base <= value < base + SIZE:
                struct.pack_into("<I", out, offset, 0xE0000000 + index * 0x100000 + (value - base))
                replaced = True
                break
        if not replaced and stack_lo <= value < stack_hi:
            struct.pack_into("<I", out, offset, 0xF0000000 + ((value - entry) & 0xFFFFFFFF))
    return bytes(out)


def canon_eax(value: int, bases: list[int], entry: int, stack_lo: int, stack_hi: int) -> str:
    for index, base in enumerate(bases):
        if base <= value < base + SIZE:
            return f"@{index}+{value - base:x}"
    if stack_lo <= value < stack_hi:
        return stack_token(value, entry)
    return f"{value & 0xFFFFFFFF:08x}"


class Graph:
    def __init__(self, vt: int) -> None:
        self.vt = vt
        self.blobs = [bytearray(SIZE) for _ in range(POOL)]
        self.fix: list[tuple[int, int, int]] = []
        self.overrides: list[tuple[int, str]] = []
        self.used = 7
        for index in range(VT_INDEX):
            self.link(index, 0, VT_INDEX)
        self.link(0, 0x18, 1)
        self.link(1, 0x24, 2)
        self.link(2, 0x374, 3)
        struct.pack_into("<I", self.blobs[4], 0x58, 100)
        self.overrides.append((GAME_LOGIC, "@4"))

    def link(self, index: int, offset: int, target: int) -> None:
        self.fix = [item for item in self.fix if not (item[0] == index and item[1] == offset)]
        self.fix.append((index, offset, target))
        # Non-zero placeholder so a later fault does not fill the same slot again.
        # plant() writes the real pointer over it.
        struct.pack_into("<I", self.blobs[index], offset, 1)

    def fresh(self) -> int | None:
        if self.used >= VT_INDEX:
            return None
        index = self.used
        self.used += 1
        self.link(index, 0, VT_INDEX)
        return index


def image_dword(emu: Emulator, address: int) -> int:
    return struct.unpack("<I", emu.read(address, 4))[0]


def bindings(emu: Emulator, fn: dict, insns) -> dict[int, dict]:
    """Direct callees only. Virtual slots are synthetic stubs keyed by displacement."""
    by_target: dict[int, dict] = {}
    size_of = {ins.address: ins.size for ins in insns}

    def add(target: int, site: dict) -> None:
        if target < 0x10000 or target == fn["va"]:
            return
        entry = by_target.setdefault(target, {"by_ret": {}, "tail": None})
        if site["tail"]:
            entry["tail"] = site
            return
        entry["by_ret"][site["site"] + size_of[site["site"]]] = site

    for site in fn["sites"]:
        if site["kind"] == "direct":
            add(site["target"], site)
    return by_target


def install_slots(emu: Emulator) -> None:
    for disp in range(0, 0x300, 4):
        def stub(active: Emulator, disp=disp) -> tuple[int, int]:
            box = ACTIVE["box"]
            ret = box["rets"][box["n"]] if box["n"] < len(box["rets"]) else 0
            box["n"] += 1
            site = ACTIVE["by_disp"].get(disp)
            cleanup = site["cleanup"] if site else 0
            argc = site["argc"] if site else 0
            esp = active.register(uc.UC_X86_REG_ESP)
            ecx = active.register(uc.UC_X86_REG_ECX)
            args = [struct.unpack("<I", active.read(esp + 4 + 4 * index, 4))[0] for index in range(argc)]
            text = f"{SLOT_ADDR[disp]:08x}:" + norm(ecx, box["bases"], box["entry"])
            text += ":" + "+".join(norm(arg, box["bases"], box["entry"]) for arg in args)
            box["log"].append(text)
            return ret, cleanup
        SLOT_ADDR[disp] = emu.bind_stub(stub, 0)


def arm_vtable(graph: Graph, fn: dict) -> None:
    ACTIVE["by_disp"] = {
        site["disp"]: site for site in fn["sites"] if site["kind"] != "direct" and site["disp"] is not None
    }
    blob = graph.blobs[VT_INDEX]
    for disp, addr in SLOT_ADDR.items():
        struct.pack_into("<I", blob, disp, addr)


def plant(emu: Emulator, graph: Graph, bases: list[int]) -> None:
    emu.reset_data()
    for index, blob in enumerate(graph.blobs):
        emu.write(bases[index], bytes(blob))
    for obj, offset, target in graph.fix:
        emu.write(bases[obj] + offset, struct.pack("<I", bases[target]))
    for address, token in graph.overrides:
        value = bases[int(token[1:])] if token.startswith("@") else int(token, 16)
        emu.write(address, struct.pack("<I", value))


def zero_cpu(emu: Emulator) -> None:
    emu.uc.reg_write(uc.UC_X86_REG_FPCW, 0x037F)
    emu.uc.reg_write(uc.UC_X86_REG_FPSW, 0)
    if hasattr(uc, "UC_X86_REG_FPTAG"):
        emu.uc.reg_write(uc.UC_X86_REG_FPTAG, 0xFFFF)
    for reg in XMM:
        emu.uc.reg_write(reg, 0)


def bind(emu: Emulator, fn: dict, table: dict[int, dict], box: dict) -> None:
    emu._function_stubs.clear()
    for target, entry in table.items():
        def stub(active: Emulator, target=target, entry=entry) -> tuple[int, int]:
            ret = box["rets"][box["n"]] if box["n"] < len(box["rets"]) else 0
            box["n"] += 1
            esp = active.register(uc.UC_X86_REG_ESP)
            ret_addr = struct.unpack("<I", active.read(esp, 4))[0]
            site = entry["by_ret"].get(ret_addr) or entry["tail"]
            cleanup = site["cleanup"] if site else 0
            argc = site["argc"] if site else 0
            ecx = active.register(uc.UC_X86_REG_ECX)
            args = [struct.unpack("<I", active.read(esp + 4 + 4 * index, 4))[0] for index in range(argc)]
            text = f"{target:08x}:" + norm(ecx, box["bases"], box["entry"])
            text += ":" + "+".join(norm(arg, box["bases"], box["entry"]) for arg in args)
            box["log"].append(text)
            return ret, cleanup
        emu.bind_function(target, stub, 0)


def run_native(emu: Emulator, fn: dict, table, graph: Graph, bases: list[int], rets: list[int], args: list[int]) -> dict:
    TRACE.clear()
    arm_vtable(graph, fn)
    plant(emu, graph, bases)
    zero_cpu(emu)
    box = {"rets": rets, "n": 0, "log": [], "bases": bases, "entry": entry_esp(len(args))}
    ACTIVE["box"] = box
    emu.write(box["entry"] - 0x180, bytes(0x180))
    bind(emu, fn, table, box)
    result = emu.call(fn["va"], args, eax=EAX_IN, ecx=bases[0], max_instructions=250_000)
    if result.stack_cleanup != (fn["ret"] or 0):
        raise RuntimeError(f"cleanup {result.stack_cleanup} != {fn['ret']}")
    return {
        "eax": result.eax,
        "log": ";".join(box["log"]) if box["log"] else "-",
        "mem": [emu.read(base, SIZE) for base in bases],
        "entry": box["entry"],
        "calls": box["n"],
    }


def which_object(bases: list[int], value: int) -> int | None:
    if value < 0x10000:
        return None
    for index, base in enumerate(bases):
        if base <= value < base + SIZE:
            return index
    return None


def fault_parts(emu: Emulator, error: Exception) -> tuple[int, int]:
    text = str(error)
    address = 0
    eip = emu.register(uc.UC_X86_REG_EIP)
    marker = "unmapped access 0x"
    if marker in text:
        address = int(text.split(marker, 1)[1].split()[0], 16)
    return address, eip


def definition(insns, eip: int, name: str, depth: int = 0):
    if depth > 4:
        return None
    prior = [ins for ins in insns if ins.address < eip]
    for ins in reversed(prior[-24:]):
        if ins.mnemonic.startswith("j") or ins.mnemonic in {"ret", "call"}:
            if name in {"eax", "ecx", "edx"}:
                return None
            continue
        if not ins.operands or ins.operands[0].type != X86_OP_REG:
            continue
        if ins.reg_name(ins.operands[0].reg) != name:
            continue
        if ins.mnemonic == "mov" and len(ins.operands) > 1:
            src = ins.operands[1]
            if src.type == X86_OP_REG:
                return definition(insns, ins.address, ins.reg_name(src.reg), depth + 1)
            return ins
        if ins.mnemonic in {"lea", "pop"}:
            return ins
        return None
    return None


def repair(emu: Emulator, fn: dict, graph: Graph, bases: list[int], address: int, eip: int, insns) -> bool:
    regs = {name: emu.register(reg) & 0xFFFFFFFF for name, reg in REG.items()}
    for name, value in regs.items():
        index = which_object(bases, value)
        if index is None or not (value <= address < value + SIZE):
            continue
        offset = address - value
        if offset % 4 == 0 and offset != 0 and dword(bytes(graph.blobs[index]), offset) == 0:
            target = graph.fresh()
            if target is None:
                return False
            graph.link(index, offset, target)
            return True
    if eip < 0x10000 or not insns:
        return False
    cursor = next((ins for ins in insns if ins.address == eip), None)
    if cursor is None or not cursor.operands or cursor.operands[0].type != X86_OP_MEM:
        # The faulting operand may be the source.
        if cursor is not None:
            for op in cursor.operands:
                if op.type == X86_OP_MEM:
                    mem = op
                    break
            else:
                return False
        else:
            return False
    else:
        mem = cursor.operands[0]
    mem = mem.mem
    if mem.base == 0 and mem.index == 0:
        slot = address & ~3
        if slot >= 0x00B00000:
            if any(item[0] == slot for item in graph.overrides):
                return False
            target = graph.fresh()
            if target is None:
                return False
            graph.overrides = [item for item in graph.overrides if item[0] != slot]
            graph.overrides.append((slot, f"@{target}"))
            return True
        return False
    if mem.base == 0:
        return False
    base_name = cursor.reg_name(mem.base)
    origin = definition(insns, eip, base_name)
    if origin is None or origin.mnemonic != "mov" or origin.operands[1].type != X86_OP_MEM:
        return False
    src = origin.operands[1].mem
    if src.base == 0 and src.index == 0 and src.disp >= 0x00B00000:
        slot = src.disp & 0xFFFFFFFF
        if any(item[0] == slot for item in graph.overrides):
            return False
        target = graph.fresh()
        if target is None:
            return False
        graph.overrides.append((slot, f"@{target}"))
        return True
    if src.base == 0:
        return False
    holder = which_object(bases, regs.get(cursor.reg_name(src.base), 0))
    if holder is None:
        return False
    offset = src.disp & 0xFFFFFFFF
    if offset > 0x7F0:
        return False
    current = dword(bytes(graph.blobs[holder]), offset)
    if current != 0:
        return False
    target = graph.fresh()
    if target is None:
        return False
    graph.link(holder, offset, target)
    return True


def fill_null(graph: Graph, bases: list[int], holder_addr: int, offset: int) -> bool:
    index = which_object(bases, holder_addr)
    if index is None or offset > 0x7F0 or offset % 4 != 0:
        return False
    if dword(bytes(graph.blobs[index]), offset) != 0:
        return False
    target = graph.fresh()
    if target is None:
        return False
    graph.link(index, offset, target)
    return True


def repair_from_trace(graph: Graph, bases: list[int], insns) -> bool:
    insn_at = {ins.address: ins for ins in insns}
    for prev_eip, prev_regs in reversed(TRACE[:-1]):
        ins = insn_at.get(prev_eip)
        if ins is None or ins.mnemonic == "call" or len(ins.operands) < 2:
            return False
        if ins.mnemonic != "mov" or ins.operands[0].type != X86_OP_REG:
            continue
        src = ins.operands[1]
        if src.type != X86_OP_MEM or not src.mem.base:
            return False
        base_val = prev_regs.get(ins.reg_name(src.mem.base), 0)
        return fill_null(graph, bases, base_val, src.mem.disp & 0xFFFFFFFF)
    return False


def succeed(emu, fn, table, graph, bases, rets, args, insns, repairs: int = 24):
    seen = set()
    for _ in range(repairs):
        try:
            native = run_native(emu, fn, table, graph, bases, rets, args)
            return native, graph
        except Exception as error:
            address, eip = fault_parts(emu, error)
            key = (address, eip, graph.used, len(graph.fix), len(graph.overrides))
            if key in seen or not (
                repair(emu, fn, graph, bases, address, eip, insns) or repair_from_trace(graph, bases, insns)
            ):
                raise RuntimeError(f"0x{fn['va']:08x} {error}") from None
            seen.add(key)
    raise RuntimeError(f"0x{fn['va']:08x} repair limit")


def differs(left: dict, right: dict, bases: list[int]) -> bool:
    if left["eax"] != right["eax"] or left["log"] != right["log"]:
        return True
    for a, b in zip(left["mem"], right["mem"]):
        if canon(a, bases, left["entry"], STACK_BASE, STACK_BASE + STACK_SIZE) != canon(
                b, bases, right["entry"], STACK_BASE, STACK_BASE + STACK_SIZE):
            return True
    return False


def second_case(emu, fn, table, graph, bases, insns, first):
    extra = graph.fresh() or 5
    trials = [
        ([bases[extra]] * 16, []),
        ([1] * 16, []),
        ([0] * 16, []),
    ]
    if (fn["ret"] or 0) == 4:
        trials.append(([0] * 16, [1]))
        trials.append(([0] * 16, [0xFFFFFFFF]))
    # Field edges that still have to return.
    clones = []
    for offset, value in ((0x20, 1), (0x20, 0xFFFFFFFF), (0x24, 0), (0x50, 1)):
        clones.append((offset, value))
    for rets, args in trials:
        trial = Graph(graph.vt)
        trial.blobs = [bytearray(blob) for blob in graph.blobs]
        trial.fix = list(graph.fix)
        trial.overrides = list(graph.overrides)
        trial.used = graph.used
        try:
            native, _used = succeed(emu, fn, table, trial, bases, rets, args, insns, repairs=6)
        except Exception:
            continue
        if differs(first, native, bases):
            return trial, rets, args, native
    for offset, value in clones:
        trial = Graph(graph.vt)
        trial.blobs = [bytearray(blob) for blob in graph.blobs]
        trial.fix = list(graph.fix)
        trial.overrides = list(graph.overrides)
        trial.used = graph.used
        struct.pack_into("<I", trial.blobs[0], offset, value)
        try:
            native, _used = succeed(emu, fn, table, trial, bases, [0] * 16, [], insns, repairs=4)
        except Exception:
            continue
        if differs(first, native, bases):
            return trial, [0] * 16, [], native
    # Drop the locomotor link.
    trial = Graph(graph.vt)
    trial.blobs = [bytearray(blob) for blob in graph.blobs]
    trial.fix = [item for item in graph.fix if not (item[0] == 2 and item[1] == 0x374)]
    trial.overrides = list(graph.overrides)
    trial.used = graph.used
    try:
        native, _used = succeed(emu, fn, table, trial, bases, [0] * 16, [], insns, repairs=4)
    except Exception:
        native = None
    if native is not None and differs(first, native, bases):
        return trial, [0] * 16, [], native
    raise RuntimeError(f"0x{fn['va']:08x} no second case")


class Checker:
    def __init__(self, executable: Path) -> None:
        self.process = subprocess.Popen(
            [str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)

    def call(self, line: str) -> str:
        assert self.process.stdin is not None and self.process.stdout is not None
        self.process.stdin.write(line + "\n")
        self.process.stdin.flush()
        result = self.process.stdout.readline().strip()
        if not result.startswith("OK "):
            raise RuntimeError(result or "checker closed")
        return result

    def close(self) -> None:
        if self.process.poll() is not None:
            return
        assert self.process.stdin is not None
        self.process.stdin.write("quit\n")
        self.process.stdin.flush()
        self.process.wait(timeout=10)


def line_for(fn, graph: Graph, rets: list[int], args: list[int]) -> str:
    words = ["run", f"{fn['va']:08x}", f"{EAX_IN:x}", str(len(args)), *[f"{arg:x}" for arg in args]]
    words.append(str(len(graph.blobs)))
    for blob in graph.blobs:
        words.append(blob.hex())
    words.append(str(len(graph.fix)))
    for index, offset, target in graph.fix:
        words.extend((str(index), f"{offset:x}", str(target)))
    words.append(str(len(graph.overrides)))
    for address, token in graph.overrides:
        words.extend((f"{address:x}", token))
    words.append(str(len(rets)))
    for ret in rets:
        # Object returns are checker-side indexes. Native rets are absolute; the
        # caller passes tokens here instead. This helper is used with tokens.
        words.append(ret if isinstance(ret, str) else f"{ret:x}")
    return " ".join(words)


def ret_tokens(rets: list[int], bases: list[int]) -> list[str]:
    tokens = []
    for ret in rets:
        for index, base in enumerate(bases):
            if ret == base:
                tokens.append(f"@{index}")
                break
        else:
            tokens.append(f"{ret & 0xFFFFFFFF:x}")
    return tokens


def parse_ok(text: str, count: int) -> tuple[int, int, int, list[int], list[bytes], str]:
    head, mem_hex, log = (part.strip() for part in text.split("|"))
    parts = head.split()
    eax = int(parts[1], 16)
    frame = int(parts[2], 16)
    entry = int(parts[3], 16)
    bases = [int(part, 16) for part in parts[4:]]
    if len(bases) != count:
        raise RuntimeError(f"base count {len(bases)} != {count}")
    width = SIZE * 2
    if len(mem_hex) != width * count:
        raise RuntimeError(f"memory hex {len(mem_hex)} != {width * count}")
    mems = [bytes.fromhex(mem_hex[index * width:(index + 1) * width]) for index in range(count)]
    return eax, frame, entry, bases, mems, log.strip() or "-"


def compare(native: dict, text: str, count: int) -> list[str]:
    eax, frame, entry, bases, mems, log = parse_ok(text, count)
    problems = []
    left = canon_eax(native["eax"], native["bases"], native["entry"], STACK_BASE, STACK_BASE + STACK_SIZE)
    right = canon_eax(eax, bases, entry, frame, frame + 0x180)
    if left != right:
        problems.append(f"eax {left} != {right}")
    if native["log"] != log:
        problems.append(f"calls {native['log']} != {log}")
    for obj, (src, dst) in enumerate(zip(native["mem"], mems)):
        a = canon(src, native["bases"], native["entry"], STACK_BASE, STACK_BASE + STACK_SIZE)
        b = canon(dst, bases, entry, frame, frame + 0x180)
        if a != b:
            offset = next(i for i, (x, y) in enumerate(zip(a, b)) if x != y)
            problems.append(
                f"obj{obj}+0x{offset:x} native {a[max(0, offset - 4):offset + 8].hex()} "
                f"port {b[max(0, offset - 4):offset + 8].hex()}"
            )
            break
    return problems


def main() -> int:
    executable = Path(sys.argv[1]) if len(sys.argv) > 1 else (
        ROOT / "Ra3Source" / "build" / "win32" / "bin" / "Release" / "ra3_aistate7ccheck.exe")
    emu = SiteEmulator()
    install_slots(emu)

    def trace_code(uc_engine, address, size, data):
        if not (0x007C0000 <= address < 0x007D0000):
            return
        TRACE.append((address, {name: uc_engine.reg_read(reg) & 0xFFFFFFFF for name, reg in REG.items()}))
        if len(TRACE) > 24:
            del TRACE[0]

    emu.uc.hook_add(unicorn.UC_HOOK_CODE, trace_code)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    bases = [emu.alloc(bytes(SIZE), alignment=256) for _ in range(POOL)]
    cases = []
    for fn in META:
        code = emu.read(fn["va"], fn["size"])
        insns = list(md.disasm(code, fn["va"]))
        table = bindings(emu, fn, insns)
        graph = Graph(fn["vtables"][0]["vt"])
        args = [0] if (fn["ret"] or 0) == 4 else []
        native = None
        used_rets = [0] * 16
        failure = None
        for used_rets in ([0] * 16, [bases[5]] * 16, [1] * 16):
            try:
                native, graph = succeed(emu, fn, table, graph, bases, used_rets, args, insns)
                break
            except RuntimeError as error:
                failure = error
                native = None
        if native is None:
            raise failure
        other, rets, other_args, second = second_case(emu, fn, table, graph, bases, insns, native)
        cases.append((fn, graph, used_rets, args, native, table))
        cases.append((fn, other, rets, other_args, second, table))
        print(f"case 0x{fn['va']:08x} calls={native['calls']}/{second['calls']}", flush=True)
    checker = Checker(executable)
    problems = []
    try:
        for index, (fn, graph, rets, args, native, table) in enumerate(cases):
            native = run_native(emu, fn, table, graph, bases, rets, args)
            tokens = ret_tokens(rets, bases)
            # Rebuild the line with checker tokens. args are immediates in these cases.
            words = ["run", f"{fn['va']:08x}", f"{EAX_IN:x}", str(len(args)), *[f"{arg:x}" for arg in args]]
            words.append(str(POOL))
            for blob in graph.blobs:
                words.append(blob.hex())
            words.append(str(len(graph.fix)))
            for obj, offset, target in graph.fix:
                words.extend((str(obj), f"{offset:x}", str(target)))
            words.append(str(len(graph.overrides)))
            for address, token in graph.overrides:
                words.extend((f"{address:x}", token))
            words.append(str(len(tokens)))
            words.extend(tokens)
            try:
                text = checker.call(" ".join(words))
            except Exception as error:
                problems.append(f"0x{fn['va']:08x} #{index} checker {error}")
                break
            native["bases"] = bases
            diff = compare(native, text, POOL)
            if diff:
                problems.append(f"0x{fn['va']:08x} #{index} " + "; ".join(diff))
                break
    finally:
        checker.close()
    if problems:
        print("\n".join(problems))
        return 1
    print(f"aistate7c oracle Release cases={len(cases)} functions={len(META)} mismatches=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())
