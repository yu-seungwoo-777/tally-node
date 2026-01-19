"""
Data models for documentation generation system.

이 모듈은 Doxygen XML 파싱 결과를 저장하기 위한 데이터 모델을 제공합니다.
"""

from dataclasses import dataclass, field
from typing import List, Optional, Dict, Any
from enum import Enum


class LayerType(Enum):
    """5계층 아키텍처 레이어 타입"""
    COMMON = "0_common"
    PRESENTATION = "1_presentation"
    APPLICATION = "2_application"
    DOMAIN = "3_domain"
    INFRASTRUCTURE = "4_infrastructure"


@dataclass
class Parameter:
    """함수 매개변수 정보"""
    name: str
    type: str
    description: str = ""
    default_value: Optional[str] = None


@dataclass
class FunctionInfo:
    """함수 정보"""
    name: str
    return_type: str
    brief: str = ""
    detailed: str = ""
    parameters: List[Parameter] = field(default_factory=list)
    return_description: str = ""
    file_path: str = ""
    line_number: int = 0
    is_static: bool = False
    is_const: bool = False
    layer: Optional[LayerType] = None
    component: Optional[str] = None


@dataclass
class MethodInfo:
    """클래스 메서드 정보"""
    name: str
    return_type: str
    brief: str = ""
    detailed: str = ""
    parameters: List[Parameter] = field(default_factory=list)
    return_description: str = ""
    is_static: bool = False
    is_const: bool = False
    is_virtual: bool = False
    visibility: str = "public"  # public, protected, private


@dataclass
class FieldInfo:
    """클래스 필드 정보"""
    name: str
    type: str
    brief: str = ""
    is_static: bool = False
    is_const: bool = False
    visibility: str = "public"


@dataclass
class ClassInfo:
    """클래스/구조체 정보"""
    name: str
    kind: str = "class"  # class, struct, interface
    brief: str = ""
    detailed: str = ""
    methods: List[MethodInfo] = field(default_factory=list)
    fields: List[FieldInfo] = field(default_factory=list)
    base_classes: List[str] = field(default_factory=list)
    file_path: str = ""
    line_number: int = 0
    layer: Optional[LayerType] = None
    component: Optional[str] = None


@dataclass
class ComponentInfo:
    """컴포넌트 정보"""
    name: str
    layer: LayerType
    brief: str = ""
    detailed: str = ""
    functions: List[FunctionInfo] = field(default_factory=list)
    classes: List[ClassInfo] = field(default_factory=list)
    responsibilities: List[str] = field(default_factory=list)
    dependencies: List[str] = field(default_factory=list)


@dataclass
class LayerInfo:
    """레이어 정보"""
    layer_type: LayerType
    name: str
    brief: str = ""
    components: List[ComponentInfo] = field(default_factory=list)
    dependencies: List[LayerType] = field(default_factory=list)
    responsibilities: List[str] = field(default_factory=list)


@dataclass
class ProjectInfo:
    """프로젝트 전체 정보"""
    name: str
    brief: str = ""
    layers: List[LayerInfo] = field(default_factory=list)
    all_functions: List[FunctionInfo] = field(default_factory=list)
    all_classes: List[ClassInfo] = field(default_factory=list)
    metadata: Dict[str, Any] = field(default_factory=dict)
