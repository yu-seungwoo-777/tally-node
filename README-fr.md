# TALLY-NODE

Systeme Tally Wi-Fi & Ethernet base sur ESP32-S3 pour la communication en temps réel avec les mélangeurs vidéo.

**Langues :** [English](README.md) | [한국어](README-ko.md) | [日本語](README-ja.md) | [简体中文](README-zh-cn.md) | [Español](README-es.md) | Français

## Vue d'ensemble

TALLY-NODE est un système TallyLight de type DIY qui réduit considérablement les coûts de production tout en maintenant une fiabilité de niveau professionnel. Conçu pour la communication en temps réel avec les mélangeurs vidéo, il prend actuellement en charge Blackmagic ATEM et vMix, avec d'autres mélangeurs à venir.

**Liens :**
- https://tally-node.com
- https://tally-node.com/purchase
- https://demo.tally-node.com

## Fonctionnalités

### Communication sans fil LoRa
- **Longue portée** : Testé jusqu'à 300m en environnement urbain (peut varier selon l'environnement)
- **Faible consommation** : Consomme moins d'énergie que le WiFi standard, prolongeant la durée de vie de la batterie RX
- **Bandes de fréquence** : Support 433MHz et 868MHz (selon les réglementations pays)
- **Signal stable** : Technologie Chirp Spread Spectrum pour une communication fiable
- **Temps réel** : Transmission instantanée de l'état tally sans délai

### Support double mode
- Connectez jusqu'à 2 mélangeurs simultanément (ATEM + vMix, vMix + vMix, etc.)
- Utilisez WiFi et Ethernet simultanément pour une configuration réseau flexible
- Mappage des canaux dans la plage 1-20

### Contrôle Web
- Interface web intuitive pour tous les réglages TX
- Configuration réseau (WiFi AP, Ethernet DHCP/Statique)
- Paramètres de connexion mélangeur (IP, Port, Protocole)
- Gestion des appareils RX (luminosité, couleurs, numéros de caméras)
- Mise à jour du firmware via l'interface web
- Journaux système et diagnostics

### Gestion des appareils RX
- Surveillance en temps réel du niveau de batterie et de la qualité du signal
- Contrôle de la luminosité LED (0-100 niveaux)
- Capacité de redémarrage à distance
- Configuration par lot pour tous les appareils RX

## Matériel

### TX (Émetteur)
- Se connecte aux mélangeurs via IP (WiFi/Ethernet)
- Alimentation USB-C et support batterie 18650
- Diffusion LoRa 433MHz / 868MHz
- Interface de contrôle Web UI
- Prend en charge jusqu'à 20 appareils RX

### RX (Récepteur)
- Se monte sur la caméra
- Reçoit les signaux tally sans fil du TX
- LED RVB pour les états Program (Rouge), Preview (Vert), Éteint
- Charge USB-C et batterie 18650
- 6-8 heures d'autonomie (testé)

## Spécifications

| Élément | TX | RX |
|---------|----|----|
| Communication | LoRa Sans fil | LoRa Sans fil |
| Portée testée | Jusqu'à 300m urbain | Jusqu'à 300m urbain |
| Mélangeurs supportés | ATEM, vMix | - |
| Caméras supportées | Jusqu'à 20 unités | - |
| Alimentation | Batterie 18650, USB-C | Batterie 18650, USB-C |
| Autonomie batterie | Jusqu'à 8 heures | Jusqu'à 8 heures |
| Réseau | Ethernet/WiFi/AP | - |
| Configuration | Web UI | Contrôle par bouton |
| Fixation | Vis 1/4 pouce | Vis 1/4 pouce |

## Mélangeurs compatibles

| Mélangeur | Statut |
|-----------|--------|
| Blackmagic ATEM | Supporté |
| vMix | Supporté |
| OBS Studio | Prévu |
| OSEE | Prévu |

### Modèles ATEM testés
- Série ATEM Television Studio
- Série ATEM Mini
- Série ATEM Constellation

## Démarrage rapide

### Configuration TX
1. Connectez l'alimentation via USB-C ou installez la batterie 18650
2. Accédez à l'interface Web : `192.168.4.1` (mode AP) ou l'IP Ethernet assignée
3. Configurez les paramètres réseau (WiFi/Ethernet)
4. Configurez la connexion du mélangeur (IP, Port, Mode)
5. Configurez la fréquence de diffusion et le SYNCWORD
6. Activez la clé de licence

### Configuration RX
1. Installez la batterie 18650 ou connectez USB-C
2. Appuyez longuement sur le bouton frontal pour définir l'ID de caméra (1-20)
3. Assurez-vous que la fréquence et le SYNCWORD correspondent au TX

## Licence

Un code de licence est requis pour activer les appareils TX. La licence détermine le nombre maximum d'appareils RX connectables. Les clés de licence n'ont pas de date d'expiration.

## Démo

Essayez la démo de l'interface Web TX : [https://demo.tally-node.com](https://demo.tally-node.com)

---

Fabriqué avec ESP32-S3
