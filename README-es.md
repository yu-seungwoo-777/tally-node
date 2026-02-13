# TALLY-NODE

Sistema Tally basado en ESP32-S3 con Wi-Fi y Ethernet para comunicacion en tiempo real con switchers de video.

**Idiomas:** [English](README.md) | [한국어](README-ko.md) | [日本語](README-ja.md) | [简体中文](README-zh-cn.md) | Español | [Français](README-fr.md)

## Resumen

TALLY-NODE es un sistema TallyLight basado en DIY que reduce significativamente los costos de produccion mientras mantiene una confiabilidad de nivel profesional. Disenado para comunicacion en tiempo real con switchers de video, actualmente soporta Blackmagic ATEM y vMix, con mas switchers proximamente.

**Enlaces:**
- Sitio Web: https://tally-node.com
- Compra: https://tally-node.com/purchase
- Demo de TX UI: https://demo.tally-node.com

## Caracteristicas

### Comunicacion Inalambrica LoRa
- **Largo Alcance**: Probado hasta 300m en entornos urbanos (puede variar segun el entorno)
- **Bajo Consumo**: Consume menos energia que el WiFi estandar, extendiendo la duracion de la bateria del RX
- **Bandas de Frecuencia**: Soporte para 433MHz y 868MHz (basado en regulaciones de cada pais)
- **Senal Estable**: Tecnologia Chirp Spread Spectrum para comunicacion confiable
- **Tiempo Real**: Transmision instantanea del estado tally sin retraso

### Soporte de Modo Dual
- Conecte hasta 2 switchers simultaneamente (ATEM + vMix, vMix + vMix, etc.)
- Use WiFi y Ethernet simultaneamente para configuracion de red flexible
- Mapeo de canales dentro del rango 1-20

### Control basado en Web
- Interfaz web intuitiva para todos los ajustes de TX
- Configuracion de red (WiFi AP, Ethernet DHCP/Estatica)
- Ajustes de conexion del switcher (IP, Puerto, Protocolo)
- Gestion de dispositivos RX (brillo, colores, numeros de camara)
- Actualizacion de firmware via interfaz web
- Logs del sistema y diagnosticos

### Gestion de Dispositivos RX
- Monitoreo en tiempo real del nivel de bateria y calidad de senal
- Control de brillo LED (0-100 niveles)
- Capacidad de reinicio remoto
- Configuracion por lotes para todos los dispositivos RX

## Hardware

### TX (Transmisor)
- Se conecta a switchers via IP (WiFi/Ethernet)
- Alimentacion USB-C y soporte de bateria 18650
- Transmision LoRa 433MHz / 868MHz
- Interfaz de control Web UI
- Soporta hasta 20 dispositivos RX

### RX (Receptor)
- Se monta en la camara
- Recibe senales tally inalambricas desde TX
- LED RGB para Program (Rojo), Preview (Verde), estados Apagado
- Carga USB-C y bateria 18650
- 6-8 horas de duracion de bateria (probado)

## Especificaciones

| Elemento | TX | RX |
|----------|----|----|
| Comunicacion | LoRa Inalambrico | LoRa Inalambrico |
| Alcance Probado | Hasta 300m urbano | Hasta 300m urbano |
| Switchers Soportados | ATEM, vMix | - |
| Camaras Soportadas | Hasta 20 unidades | - |
| Alimentacion | Bateria 18650, USB-C | Bateria 18650, USB-C |
| Duracion de Bateria | Hasta 8 horas | Hasta 8 horas |
| Red | Ethernet/WiFi/AP | - |
| Configuracion | Web UI | Control por Boton |
| Montaje | Tornillo de 1/4 pulgada | Tornillo de 1/4 pulgada |

## Switchers Compatibles

| Switcher | Estado |
|----------|--------|
| Blackmagic ATEM | Soportado |
| vMix | Soportado |
| OBS Studio | Planificado |
| OSEE | Planificado |

### Modelos ATEM Probados
- Serie ATEM Television Studio
- Serie ATEM Mini
- Serie ATEM Constellation

## Inicio Rapido

### Configuracion de TX
1. Conecte la alimentacion via USB-C o instale bateria 18650
2. Acceda a Web UI: `192.168.4.1` (modo AP) o IP Ethernet asignada
3. Configure los ajustes de red (WiFi/Ethernet)
4. Configure la conexion del switcher (IP, Puerto, Modo)
5. Configure la frecuencia de transmision y SYNCWORD
6. Active la clave de licencia

### Configuracion de RX
1. Instale bateria 18650 o conecte USB-C
2. Presione largo el boton frontal para establecer ID de camara (1-20)
3. Asegurese que la frecuencia y SYNCWORD coincidan con TX

## Licencia

Se requiere un codigo de licencia para activar dispositivos TX. La licencia determina el numero maximo de dispositivos RX conectables. Las claves de licencia no tienen fecha de vencimiento.

## Demo

Pruebe la demo de TX Web UI: [https://demo.tally-node.com](https://demo.tally-node.com)

---

Hecho con ESP32-S3
