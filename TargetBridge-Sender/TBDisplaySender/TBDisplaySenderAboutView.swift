import AppKit
import SwiftUI

struct TBDisplaySenderAboutView: View {
    @ObservedObject var service: TBDisplaySenderService
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        VStack(alignment: .leading, spacing: 20) {
            HStack(alignment: .center, spacing: 16) {
                Image(nsImage: NSApplication.shared.applicationIconImage)
                    .resizable()
                    .frame(width: 72, height: 72)
                    .accessibilityHidden(true)

                VStack(alignment: .leading, spacing: 6) {
                    Text(TBDisplaySenderL10n.appName(service.language))
                        .font(.largeTitle.weight(.bold))
                    Text(aboutSubtitle)
                        .font(.callout)
                        .foregroundStyle(.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                    versionChip
                }

                Spacer(minLength: 12)
            }

            TBGroupedSection(title: projectTitle) {
                VStack(alignment: .leading, spacing: 14) {
                    Text(projectDescription)
                        .fixedSize(horizontal: false, vertical: true)

                    HStack(spacing: 10) {
                        Link(destination: URL(string: "https://github.com/swellweb/targetBridge")!) {
                            Label(githubTitle, systemImage: "link")
                        }
                        .tbButtonStyle(prominent: true)

                        Link(destination: URL(string: "https://github.com/swellweb/targetBridge/releases/latest")!) {
                            Label(releaseTitle, systemImage: "shippingbox")
                        }
                        .tbButtonStyle()
                    }
                }
                .padding(14)
            }

            TBGroupedSection(title: creditsTitle) {
                Text(creditsBody)
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .fixedSize(horizontal: false, vertical: true)
                    .padding(14)
            }

            HStack {
                Spacer()
                Button(closeTitle) {
                    dismiss()
                }
                .tbButtonStyle()
            }
        }
        .padding(24)
        .frame(minWidth: 620, minHeight: 420)
    }

    private var versionChip: some View {
        Text("\(versionTitle) \(TBDisplaySenderBuildInfo.versionDisplay)")
            .font(.system(.footnote, design: .monospaced))
            .foregroundStyle(.secondary)
            .padding(.horizontal, 10)
            .padding(.vertical, 4)
            .background(.quinary, in: Capsule(style: .continuous))
            .textSelection(.enabled)
    }

    private var aboutSubtitle: String {
        switch service.language {
        case .italian: return "Riporta l'idea di Target Display Mode nel mondo Apple Silicon con una pipeline diretta Mac-to-Mac."
        case .english: return "Brings the Target Display Mode idea back to Apple Silicon with a direct Mac-to-Mac display pipeline."
        case .german: return "Bringt die Idee von Target Display Mode mit einer direkten Mac-zu-Mac-Display-Pipeline zurück auf Apple Silicon."
        case .french: return "Redonne vie à l’idée du Target Display Mode sur Apple Silicon grâce à une chaîne d’affichage directe de Mac à Mac."
        case .chinese: return "通过直接的 Mac 到 Mac 显示管线，把 Target Display Mode 的理念带回 Apple Silicon 时代。"
        }
    }

    private var projectTitle: String {
        switch service.language {
        case .italian: return "Progetto"
        case .english: return "Project"
        case .german: return "Projekt"
        case .french: return "Projet"
        case .chinese: return "项目"
        }
    }

    private var projectDescription: String {
        switch service.language {
        case .italian: return "TargetBridge cattura il desktop o il monitor virtuale sul Mac sender, codifica lo stream e lo presenta su un iMac receiver via Thunderbolt Bridge, Ethernet o USB."
        case .english: return "TargetBridge captures the sender desktop or virtual display, encodes the stream, and presents it on an iMac receiver over Thunderbolt Bridge, Ethernet, or USB."
        case .german: return "TargetBridge erfasst den Sender-Desktop oder das virtuelle Display, kodiert den Stream und zeigt ihn auf einem iMac-Empfänger über Thunderbolt Bridge, Ethernet oder USB an."
        case .french: return "TargetBridge capture le bureau ou l’écran virtuel du sender, encode le flux et l’affiche sur un iMac receiver via Thunderbolt Bridge, Ethernet ou USB."
        case .chinese: return "TargetBridge 会捕获发送端 Mac 的桌面或虚拟显示器，对流进行编码，并通过 Thunderbolt Bridge、以太网或 USB 在 iMac 接收端上显示。"
        }
    }

    private var creditsTitle: String {
        switch service.language {
        case .italian: return "Crediti"
        case .english: return "Credits"
        case .german: return "Mitwirkende"
        case .french: return "Crédits"
        case .chinese: return "致谢"
        }
    }

    private var creditsBody: String {
        switch service.language {
        case .italian: return "Creato da swellweb con il supporto della community open source TargetBridge. Contributi chiave da tester e collaboratori come ThomasWaldmann, DrDavidL, potar712 e altri membri della community."
        case .english: return "Created by swellweb with support from the TargetBridge open-source community. Key contributions from testers and collaborators such as ThomasWaldmann, DrDavidL, potar712, and other community members."
        case .german: return "Erstellt von swellweb mit Unterstützung der TargetBridge-Open-Source-Community. Wichtige Beiträge von Testern und Mitwirkenden wie ThomasWaldmann, DrDavidL, potar712 und weiteren Community-Mitgliedern."
        case .french: return "Créé par swellweb avec le soutien de la communauté open source TargetBridge. Contributions essentielles de testeurs et collaborateurs comme ThomasWaldmann, DrDavidL, potar712 et d’autres membres de la communauté."
        case .chinese: return "由 swellweb 在 TargetBridge 开源社区的支持下创建。ThomasWaldmann、DrDavidL、potar712 以及其他社区成员提供了重要测试和协作贡献。"
        }
    }

    private var githubTitle: String {
        switch service.language {
        case .italian: return "GitHub"
        case .english: return "GitHub"
        case .german: return "GitHub"
        case .french: return "GitHub"
        case .chinese: return "GitHub"
        }
    }

    private var releaseTitle: String {
        switch service.language {
        case .italian: return "Ultima release"
        case .english: return "Latest release"
        case .german: return "Letztes Release"
        case .french: return "Dernière version"
        case .chinese: return "最新发布"
        }
    }

    private var versionTitle: String {
        switch service.language {
        case .italian: return "Versione"
        case .english: return "Version"
        case .german: return "Version"
        case .french: return "Version"
        case .chinese: return "版本"
        }
    }

    private var closeTitle: String {
        switch service.language {
        case .italian: return "Chiudi"
        case .english: return "Close"
        case .german: return "Schließen"
        case .french: return "Fermer"
        case .chinese: return "关闭"
        }
    }
}
