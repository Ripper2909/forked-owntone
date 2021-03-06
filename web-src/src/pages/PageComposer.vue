<template>
  <div>
    <content-with-heading>
      <template slot="heading-left">
        <p class="title is-4">{{ name }}</p>
      </template>
      <template slot="heading-right">
        <div class="buttons is-centered">
          <a class="button is-small is-light is-rounded" @click="show_composer_details_modal = true">
            <span class="icon"><i class="mdi mdi-dots-horizontal mdi-18px"></i></span>
          </a>
          <a class="button is-small is-dark is-rounded" @click="play">
            <span class="icon"><i class="mdi mdi-shuffle"></i></span> <span>Shuffle</span>
          </a>
        </div>
      </template>
      <template slot="content">
        <p class="heading has-text-centered-mobile">{{ composer_albums.total }} albums | <a class="has-text-link" @click="open_tracks">tracks</a></p>
        <list-item-albums v-for="album in composer_albums.items" :key="album.id" :album="album" @click="open_album(album)">
          <template slot="actions">
            <a @click="open_dialog(album)">
              <span class="icon has-text-dark"><i class="mdi mdi-dots-vertical mdi-18px"></i></span>
            </a>
          </template>
        </list-item-albums>
        <modal-dialog-album :show="show_details_modal" :album="selected_album" @close="show_details_modal = false" />
        <modal-dialog-composer :show="show_composer_details_modal" :composer="{ 'name': name }" @close="show_composer_details_modal = false" />
      </template>
    </content-with-heading>
  </div>
</template>

<script>
import { LoadDataBeforeEnterMixin } from './mixin'
import ContentWithHeading from '@/templates/ContentWithHeading'
import ListItemAlbums from '@/components/ListItemAlbum'
import ModalDialogAlbum from '@/components/ModalDialogAlbum'
import ModalDialogComposer from '@/components/ModalDialogComposer'
import webapi from '@/webapi'

const composerData = {
  load: function (to) {
    return webapi.library_composer(to.params.composer)
  },

  set: function (vm, response) {
    vm.name = vm.$route.params.composer
    vm.composer_albums = response.data.albums
  }
}

export default {
  name: 'PageComposer',
  mixins: [LoadDataBeforeEnterMixin(composerData)],
  components: { ContentWithHeading, ListItemAlbums, ModalDialogAlbum, ModalDialogComposer },

  data () {
    return {
      name: '',
      composer_albums: { items: [] },
      show_details_modal: false,
      selected_album: {},

      show_composer_details_modal: false
    }
  },

  computed: {
    index_list () {
      return [...new Set(this.composer_albums.items
        .map(album => album.name_sort.charAt(0).toUpperCase()))]
    }
  },

  methods: {
    open_tracks: function () {
      this.show_details_modal = false
      this.$router.push({ name: 'ComposerTracks', params: { composer: this.name } })
    },

    play: function () {
      webapi.player_play_expression('composer is "' + this.name + '" and media_kind is music', true)
    },

    open_album: function (album) {
      this.$router.push({ path: '/music/albums/' + album.id })
    },

    open_dialog: function (album) {
      this.selected_album = album
      this.show_details_modal = true
    }
  }
}
</script>

<style>
</style>
