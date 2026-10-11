/*
 * Audacity: A Digital Audio Editor
 *
 * Regression tests for filenames on imported label tracks.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QString>
#include <QTemporaryDir>

#include "au3interactiontestbase.h"
#include "importexport/labels/internal/au3/au3labelsimporter.h"

#include "au3-label-track/LabelTrack.h"
#include "au3-track/Track.h"
#include "au3wrap/internal/domaccessor.h"
#include "au3wrap/internal/wxtypes_convert.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Truly;

namespace au::trackedit {
namespace {

muse::io::path_t writeLabelFile(const QTemporaryDir& directory, const QString& filename)
{
    const QString path = QDir(directory.path()).filePath(filename);
    QFile file(path);
    const bool opened = file.open(QIODevice::WriteOnly | QIODevice::Text);
    EXPECT_TRUE(opened) << "Cannot create test label file";
    if (opened) {
        const QByteArray labels("0.100000\t0.200000\tClick A\n0.300000\t0.300000\tClick B\n");
        EXPECT_EQ(file.write(labels), labels.size());
        file.close();
    }
    return muse::io::path_t(path);
}

class Au3LabelsImporterTests : public Au3InteractionTestBase
{
public:
    void SetUp() override
    {
        m_importer = std::make_unique<importexport::Au3LabelsImporter>(muse::modularity::globalCtx());

        m_globalContext = std::make_shared<NiceMock<context::GlobalContextMock>>();
        m_currentProject = std::make_shared<NiceMock<project::AudacityProjectMock>>();
        m_trackEditProject = std::make_shared<NiceMock<TrackeditProjectMock>>();

        m_importer->globalContext.set(m_globalContext);

        ON_CALL(*m_globalContext, currentProject())
        .WillByDefault(Return(m_currentProject));
        ON_CALL(*m_globalContext, currentTrackeditProject())
        .WillByDefault(Return(m_trackEditProject));

        ON_CALL(*m_currentProject, trackeditProject())
        .WillByDefault(Return(m_trackEditProject));

        initTestProject();
    }

protected:
    std::unique_ptr<importexport::Au3LabelsImporter> m_importer;
};

TEST_F(Au3LabelsImporterTests, ImportedTxtFilenameBecomesTrackTitleAndIsNotifiedToUi)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const auto filePath = writeLabelFile(directory, "click_example_truth_buzz_with_snaps.labels.txt");

    // Track notifications drive the Audacity 4 track panel, so assert the
    // title in the actual event as well as the underlying AU3 track.
    EXPECT_CALL(*m_trackEditProject, notifyAboutTrackAdded(
                    Truly([](const Track& track) {
        return track.title.toStdString() == "click_example_truth_buzz_with_snaps.labels"
               && track.type == TrackType::Label;
    }))).Times(1);
    EXPECT_CALL(*m_trackEditProject, notifyAboutLabelAdded(_)).Times(2);

    ASSERT_TRUE(m_importer->importData(filePath).success());

    const auto& tracks = Au3TrackList::Get(projectRef());
    ASSERT_EQ(tracks.Size(), 1u);
    const Au3LabelTrack* track = DomAccessor::findLabelTrackByIndex(projectRef(), 0);
    ASSERT_NE(track, nullptr);
    EXPECT_EQ(wxToStdString(track->GetName()), "click_example_truth_buzz_with_snaps.labels");
    EXPECT_EQ(track->GetNumLabels(), 2u);
}

TEST_F(Au3LabelsImporterTests, EachImportedFileKeepsItsOwnTitle)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    const auto first = writeLabelFile(directory, "clicks.truth.labels.txt");
    const auto second = writeLabelFile(directory, "snaps.interference.txt");

    EXPECT_CALL(*m_trackEditProject, notifyAboutTrackAdded(_)).Times(2);
    EXPECT_CALL(*m_trackEditProject, notifyAboutLabelAdded(_)).Times(4);

    ASSERT_TRUE(m_importer->importData(first).success());
    ASSERT_TRUE(m_importer->importData(second).success());

    const auto& tracks = Au3TrackList::Get(projectRef());
    ASSERT_EQ(tracks.Size(), 2u);

    const Au3LabelTrack* firstTrack = DomAccessor::findLabelTrackByIndex(projectRef(), 0);
    const Au3LabelTrack* secondTrack = DomAccessor::findLabelTrackByIndex(projectRef(), 1);
    ASSERT_NE(firstTrack, nullptr);
    ASSERT_NE(secondTrack, nullptr);
    EXPECT_EQ(wxToStdString(firstTrack->GetName()), "clicks.truth.labels");
    EXPECT_EQ(wxToStdString(secondTrack->GetName()), "snaps.interference");
    EXPECT_EQ(firstTrack->GetNumLabels(), 2u);
    EXPECT_EQ(secondTrack->GetNumLabels(), 2u);
}

TEST_F(Au3LabelsImporterTests, NonAsciiAndSpacesInFileNameArePreserved)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    const auto filePath = writeLabelFile(directory, QString::fromUtf8("海豚 clicks.labels.txt"));
    EXPECT_CALL(*m_trackEditProject, notifyAboutTrackAdded(_)).Times(1);
    EXPECT_CALL(*m_trackEditProject, notifyAboutLabelAdded(_)).Times(2);

    ASSERT_TRUE(m_importer->importData(filePath).success());

    const Au3LabelTrack* track = DomAccessor::findLabelTrackByIndex(projectRef(), 0);
    ASSERT_NE(track, nullptr);
    EXPECT_EQ(wxToStdString(track->GetName()), "海豚 clicks.labels");
}

TEST_F(Au3LabelsImporterTests, MissingFileDoesNotCreateOrNotifyAboutTrack)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());

    const auto path = muse::io::path_t(QDir(directory.path()).filePath("missing.labels.txt"));
    EXPECT_CALL(*m_trackEditProject, notifyAboutTrackAdded(_)).Times(0);

    EXPECT_FALSE(m_importer->importData(path).success());
    EXPECT_EQ(Au3TrackList::Get(projectRef()).Size(), 0u);
}

} // namespace
} // namespace au::trackedit
